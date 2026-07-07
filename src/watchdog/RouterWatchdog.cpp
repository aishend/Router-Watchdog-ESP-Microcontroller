#include "RouterWatchdog.h"

#include <Arduino.h>

#include "../backend/BackendClient.h"
#include "../commands/CommandManager.h"
#include "../config/AppConfig.h"
#include "../drivers/Relay.h"
#include "../logging/Logger.h"
#include "../network/NetworkManager.h"

namespace
{
    enum class RecoveryState
    {
        Monitoring,
        PowerOff,
        RecoveryWait,
    };

    struct WatchdogState
    {
        uint8_t consecutive_failures = 0;
        unsigned long last_check_ms = 0;
        unsigned long recovery_until_ms = 0;
        bool last_wifi_connected = false;
        bool last_internet_connected = false;
        bool have_last_status = false;
        RecoveryState recovery_state = RecoveryState::Monitoring;
    };

    WatchdogState state;

    bool hasElapsed(unsigned long now, unsigned long deadline)
    {
        return static_cast<long>(now - deadline) >= 0;
    }

    bool checkIntervalElapsed(unsigned long now)
    {
        return state.last_check_ms == 0 ||
               now - state.last_check_ms >= AppConfig::CHECK_INTERVAL_MS;
    }

    void logNetworkStatus(const NetworkStatus &status)
    {
        if (status.wifi_connected)
        {
            Log::infof(
                "NETWORK",
                "Wi-Fi connected | IP=%s | Gateway=%s | Internet=%s | Failures=%u",
                status.ip_address.toString().c_str(),
                status.gateway_address.toString().c_str(),
                status.internet_connected ? "OK" : "DOWN",
                state.consecutive_failures);
        }
        else
        {
            Log::infof("NETWORK", "Wi-Fi not connected | Failures=%u", state.consecutive_failures);
        }
    }

    void startRecovery(unsigned long now)
    {
        Log::warn("WATCHDOG", "Recovery triggered");

        Log::info("RECOVERY", "relay ON, router power removed");
        Relay::turnOn();

        state.recovery_state = RecoveryState::PowerOff;
        state.recovery_until_ms = now + AppConfig::ROUTER_POWER_OFF_TIME_MS;
    }

    void continueRecovery(unsigned long now)
    {
        if (state.recovery_state == RecoveryState::Monitoring ||
            !hasElapsed(now, state.recovery_until_ms))
        {
            return;
        }

        if (state.recovery_state == RecoveryState::PowerOff)
        {
            Log::info("RECOVERY", "relay OFF, router power restored");
            Relay::turnOff();

            Log::infof("RECOVERY", "waiting for router recovery (%lu ms)", AppConfig::ROUTER_RECOVERY_WAIT_TIME_MS);

            state.recovery_state = RecoveryState::RecoveryWait;
            state.recovery_until_ms = now + AppConfig::ROUTER_RECOVERY_WAIT_TIME_MS;
            return;
        }

        if (state.recovery_state == RecoveryState::RecoveryWait)
        {
            state.consecutive_failures = 0;
            state.have_last_status = false;
            state.last_check_ms = 0;
            state.recovery_state = RecoveryState::Monitoring;

            Log::info("WATCHDOG", "Monitoring resumed");
            CommandManager::notifyRouterRecoveryFinished();
        }
    }

    void updateFailureCount(bool check_failed)
    {
        if (check_failed)
        {
            state.consecutive_failures++;

            Log::warnf(
                "WATCHDOG",
                "Failure count %u/%u",
                state.consecutive_failures,
                AppConfig::MAX_CONSECUTIVE_FAILURES);
        }
        else if (state.consecutive_failures > 0)
        {
            state.consecutive_failures = 0;
            Log::info("WATCHDOG", "Failure count cleared");
        }
    }

    bool shouldLogStatus(const NetworkStatus &status, bool check_failed)
    {
        return !state.have_last_status ||
               status.wifi_connected != state.last_wifi_connected ||
               status.internet_connected != state.last_internet_connected ||
               check_failed;
    }

    void rememberStatus(const NetworkStatus &status)
    {
        state.last_wifi_connected = status.wifi_connected;
        state.last_internet_connected = status.internet_connected;
        state.have_last_status = true;
    }
}

namespace RouterWatchdog
{
    void begin()
    {
        Log::info("WATCHDOG", "Monitoring started");
    }

    bool requestRouterReboot()
    {
        if (state.recovery_state != RecoveryState::Monitoring)
        {
            Log::warn("WATCHDOG", "Recovery request ignored, already recovering");
            return false;
        }

        startRecovery(millis());
        return true;
    }

    void tick(unsigned long now)
    {
        continueRecovery(now);

        if (state.recovery_state != RecoveryState::Monitoring ||
            !checkIntervalElapsed(now))
        {
            return;
        }

        state.last_check_ms = now;

        NetworkStatus status = NetworkManager::checkStatus();
        bool check_failed = !status.wifi_connected || !status.internet_connected;

        updateFailureCount(check_failed);

        if (shouldLogStatus(status, check_failed))
        {
            logNetworkStatus(status);
        }

        rememberStatus(status);

        HeartbeatResponse response = BackendClient::sendHeartbeat(
            status,
            state.consecutive_failures);

        if (response.command.has_command)
        {
            CommandManager::handleCommand(response.command);
        }

        if (state.consecutive_failures >= AppConfig::MAX_CONSECUTIVE_FAILURES)
        {
            requestRouterReboot();
        }
    }
}
