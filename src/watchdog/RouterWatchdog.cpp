#include "RouterWatchdog.h"

#include <Arduino.h>

#include "../commands/CommandManager.h"
#include "../config/AppConfig.h"
#include "../drivers/Relay.h"
#include "../mqtt/MqttClient.h"
#include "../network/NetworkManager.h"

namespace
{
    enum class RecoveryState
    {
        Monitoring,
        PowerOff,
        RecoveryWait,
    };

    bool hasElapsed(unsigned long now, unsigned long deadline)
    {
        return static_cast<long>(now - deadline) >= 0;
    }

    class WatchdogController
    {
    public:
        void begin()
        {
            Serial.println("[WATCHDOG] Monitoring started");
        }

        bool requestRouterReboot()
        {
            if (!canRequestRouterReboot())
            {
                Serial.println("[WATCHDOG] WARN Recovery request ignored, already recovering");
                return false;
            }

            startRecovery(millis());
            return true;
        }

        bool canRequestRouterReboot() const
        {
            return recovery_state == RecoveryState::Monitoring && !recovery_confirmation_pending;
        }

        void tick(unsigned long now)
        {
            continueRecovery(now);

            if (recovery_state != RecoveryState::Monitoring ||
                !checkIntervalElapsed(now))
            {
                return;
            }

            last_check_ms = now;

            NetworkStatus status = NetworkManager::checkStatus();
            bool check_failed = !status.wifi_connected || !status.internet_connected;

            updateFailureCount(check_failed);

            if (shouldLogStatus(status, check_failed))
            {
                logNetworkStatus(status);
            }

            rememberStatus(status);

            MqttClient::publishHeartbeat(status, consecutive_failures);

            if (recovery_confirmation_pending && !check_failed)
            {
                Serial.println("[WATCHDOG] Router recovery confirmed");
                recovery_confirmation_pending = false;
                CommandManager::notifyRouterRecoveryFinished();
            }

            if (consecutive_failures >= AppConfig::MAX_CONSECUTIVE_FAILURES)
            {
                requestRouterReboot();
            }
        }

    private:
        uint8_t consecutive_failures = 0;
        unsigned long last_check_ms = 0;
        unsigned long recovery_until_ms = 0;
        bool last_wifi_connected = false;
        bool last_internet_connected = false;
        bool have_last_status = false;
        bool recovery_confirmation_pending = false;
        RecoveryState recovery_state = RecoveryState::Monitoring;

        bool checkIntervalElapsed(unsigned long now) const
        {
            return last_check_ms == 0 ||
                   now - last_check_ms >= AppConfig::CHECK_INTERVAL_MS;
        }

        void logNetworkStatus(const NetworkStatus &status) const
        {
            if (status.wifi_connected)
            {
                Serial.printf(
                    "[NETWORK] Wi-Fi connected | IP=%s | Gateway=%s | Internet=%s | Failures=%u\n",
                    status.ip_address.toString().c_str(),
                    status.gateway_address.toString().c_str(),
                    status.internet_connected ? "OK" : "DOWN",
                    consecutive_failures);
            }
            else
            {
                Serial.printf("[NETWORK] Wi-Fi not connected | Failures=%u\n", consecutive_failures);
            }
        }

        void startRecovery(unsigned long now)
        {
            Serial.println("[WATCHDOG] WARN Recovery triggered");

            Serial.println("[RECOVERY] relay ON, router power removed");
            Relay::turnOn();

            recovery_state = RecoveryState::PowerOff;
            recovery_until_ms = now + AppConfig::ROUTER_POWER_OFF_TIME_MS;
        }

        void continueRecovery(unsigned long now)
        {
            if (recovery_state == RecoveryState::Monitoring ||
                !hasElapsed(now, recovery_until_ms))
            {
                return;
            }

            if (recovery_state == RecoveryState::PowerOff)
            {
                Serial.println("[RECOVERY] relay OFF, router power restored");
                Relay::turnOff();

                Serial.printf("[RECOVERY] waiting for router recovery (%lu ms)\n", AppConfig::ROUTER_RECOVERY_WAIT_TIME_MS);

                recovery_state = RecoveryState::RecoveryWait;
                recovery_until_ms = now + AppConfig::ROUTER_RECOVERY_WAIT_TIME_MS;
                return;
            }

            if (recovery_state == RecoveryState::RecoveryWait)
            {
                consecutive_failures = 0;
                have_last_status = false;
                last_check_ms = 0;
                recovery_state = RecoveryState::Monitoring;
                recovery_confirmation_pending = true;

                Serial.println("[WATCHDOG] Monitoring resumed, waiting for network confirmation");
            }
        }

        void updateFailureCount(bool check_failed)
        {
            if (check_failed)
            {
                consecutive_failures++;

                Serial.printf(
                    "[WATCHDOG] WARN Failure count %u/%u\n",
                    consecutive_failures,
                    AppConfig::MAX_CONSECUTIVE_FAILURES);
            }
            else if (consecutive_failures > 0)
            {
                consecutive_failures = 0;
                Serial.println("[WATCHDOG] Failure count cleared");
            }
        }

        bool shouldLogStatus(const NetworkStatus &status, bool check_failed) const
        {
            return !have_last_status ||
                   status.wifi_connected != last_wifi_connected ||
                   status.internet_connected != last_internet_connected ||
                   check_failed;
        }

        void rememberStatus(const NetworkStatus &status)
        {
            last_wifi_connected = status.wifi_connected;
            last_internet_connected = status.internet_connected;
            have_last_status = true;
        }
    };

    WatchdogController watchdog;
}

namespace RouterWatchdog
{
    void begin()
    {
        watchdog.begin();
    }

    bool requestRouterReboot()
    {
        return watchdog.requestRouterReboot();
    }

    bool canRequestRouterReboot()
    {
        return watchdog.canRequestRouterReboot();
    }

    void tick(unsigned long now)
    {
        watchdog.tick(now);
    }
}
