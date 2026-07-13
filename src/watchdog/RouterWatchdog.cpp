#include "RouterWatchdog.h"

#include <Arduino.h>

#include "../config/AppConfig.h"
#include "../drivers/Relay.h"
#include "../mqtt/MqttClient.h"
#include "../network/NetworkManager.h"
#include "../operations/OperationCoordinator.h"

namespace
{
    enum class RecoveryState
    {
        Monitoring,
        PowerOff,
        RecoveryCooldown,
    };

    class WatchdogController
    {
    public:
        void begin()
        {
            Serial.println("[WATCHDOG] Monitoring started");
        }

        void tick(unsigned long now)
        {
            restoreRouterPowerWhenDue(now);
            consumeNetworkResult(now);
            startCooldownRetryWhenDue(now);
            scheduleNetworkCheck(now);
        }

        bool requestRouterReboot()
        {
            if (recoveryState != RecoveryState::Monitoring)
            {
                return false;
            }

            if (!OperationCoordinator::isActive(DestructiveOperation::RouterRecovery) &&
                !OperationCoordinator::tryStart(DestructiveOperation::RouterRecovery))
            {
                return false;
            }

            startRecovery(millis());
            return true;
        }

        bool canRequestRouterReboot() const
        {
            return recoveryState == RecoveryState::Monitoring && OperationCoordinator::isIdle();
        }

        bool canStartFirmwareUpdate() const
        {
            return recoveryState == RecoveryState::Monitoring;
        }

        bool isRecovering() const { return recoveryState != RecoveryState::Monitoring; }
        bool isCooldownActive() const { return recoveryState == RecoveryState::RecoveryCooldown; }

    private:
        RecoveryState recoveryState = RecoveryState::Monitoring;
        uint8_t consecutiveFailures = 0;
        uint8_t fastRecoveryAttempts = 0;
        unsigned long stateDeadlineMs = 0;
        unsigned long lastCheckMs = 0;
        bool checkRequested = false;
        bool haveLastStatus = false;
        NetworkStatus lastStatus;

        static bool deadlineReached(unsigned long now, unsigned long deadline)
        {
            return static_cast<long>(now - deadline) >= 0;
        }

        unsigned long recoveryInterval() const
        {
            return fastRecoveryAttempts < AppConfig::FAST_RECOVERY_ATTEMPTS
                       ? AppConfig::FAST_RECOVERY_INTERVAL_MS
                       : AppConfig::SLOW_RECOVERY_INTERVAL_MS;
        }

        void restoreRouterPowerWhenDue(unsigned long now)
        {
            if (recoveryState == RecoveryState::PowerOff && deadlineReached(now, stateDeadlineMs))
            {
                Relay::turnOff();
                recoveryState = RecoveryState::RecoveryCooldown;
                stateDeadlineMs = now + recoveryInterval();
                lastCheckMs = 0;
                Serial.printf("[RECOVERY] Router power restored; observation window=%lu ms\n", recoveryInterval());
            }
        }

        void startCooldownRetryWhenDue(unsigned long now)
        {
            if (recoveryState == RecoveryState::RecoveryCooldown && deadlineReached(now, stateDeadlineMs))
            {
                startRecovery(now);
            }
        }

        void scheduleNetworkCheck(unsigned long now)
        {
            if (checkRequested)
            {
                return;
            }

            if (lastCheckMs != 0 && now - lastCheckMs < AppConfig::CHECK_INTERVAL_MS)
            {
                return;
            }

            lastCheckMs = now;
            checkRequested = true;
            NetworkManager::requestStatusCheck();
        }

        void consumeNetworkResult(unsigned long now)
        {
            NetworkStatus status;
            if (!NetworkManager::takeStatusResult(status))
            {
                return;
            }

            checkRequested = false;
            const bool healthy = status.wifi_connected && status.internet_connected;
            updateFailureCount(!healthy);
            logStatusWhenUseful(status, !healthy);
            lastStatus = status;
            haveLastStatus = true;
            MqttClient::publishHeartbeat(status, consecutiveFailures);

            if (healthy && recoveryState == RecoveryState::RecoveryCooldown)
            {
                finishRecovery();
                return;
            }

            if (!healthy && recoveryState == RecoveryState::Monitoring &&
                consecutiveFailures >= AppConfig::MAX_CONSECUTIVE_FAILURES &&
                OperationCoordinator::isIdle())
            {
                OperationCoordinator::tryStart(DestructiveOperation::RouterRecovery);
                startRecovery(now);
            }
        }

        void startRecovery(unsigned long now)
        {
            if (fastRecoveryAttempts < AppConfig::FAST_RECOVERY_ATTEMPTS)
            {
                fastRecoveryAttempts++;
            }

            Relay::turnOn();
            recoveryState = RecoveryState::PowerOff;
            stateDeadlineMs = now + AppConfig::ROUTER_POWER_OFF_TIME_MS;
            Serial.printf("[RECOVERY] Relay ON; fast attempt=%u/%u\n",
                          fastRecoveryAttempts, AppConfig::FAST_RECOVERY_ATTEMPTS);
        }

        void finishRecovery()
        {
            consecutiveFailures = 0;
            fastRecoveryAttempts = 0;
            recoveryState = RecoveryState::Monitoring;
            OperationCoordinator::finish(DestructiveOperation::RouterRecovery);
            Serial.println("[RECOVERY] Network confirmed; recovery state reset");
        }

        void updateFailureCount(bool failed)
        {
            if (failed && consecutiveFailures < AppConfig::MAX_CONSECUTIVE_FAILURES)
            {
                consecutiveFailures++;
            }
            else if (!failed)
            {
                consecutiveFailures = 0;
            }
        }

        void logStatusWhenUseful(const NetworkStatus &status, bool failed) const
        {
            if (haveLastStatus && !failed &&
                status.wifi_connected == lastStatus.wifi_connected &&
                status.internet_connected == lastStatus.internet_connected)
            {
                return;
            }

            Serial.printf("[NETWORK] Wi-Fi=%s Internet=%s Failures=%u\n",
                          status.wifi_connected ? "OK" : "DOWN",
                          status.internet_connected ? "OK" : "DOWN",
                          consecutiveFailures);
        }
    };

    WatchdogController watchdog;
}

namespace RouterWatchdog
{
    void begin() { watchdog.begin(); }
    void tick(unsigned long now) { watchdog.tick(now); }
    bool requestRouterReboot() { return watchdog.requestRouterReboot(); }
    bool canRequestRouterReboot() { return watchdog.canRequestRouterReboot(); }
    bool canStartFirmwareUpdate() { return watchdog.canStartFirmwareUpdate(); }
    bool isRecovering() { return watchdog.isRecovering(); }
    bool isCooldownActive() { return watchdog.isCooldownActive(); }
}
