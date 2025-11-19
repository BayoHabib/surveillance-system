/**
 * Notification Manager for Surveillance System
 * Handles browser push notifications with sound, vibration, and badge support
 */

class NotificationManager {
    constructor() {
        this.settings = {
            sound: true,
            vibration: true,
            badge: true,
            permission: 'default'
        };
        
        this.badgeCount = 0;
        this.loadSettings();
        this.checkSupport();
    }

    /**
     * Check if browser supports notifications
     */
    checkSupport() {
        if (!('Notification' in window)) {
            console.error('[NotificationManager] Browser does not support notifications');
            return false;
        }
        return true;
    }

    /**
     * Load settings from localStorage
     */
    loadSettings() {
        const saved = localStorage.getItem('notificationSettings');
        if (saved) {
            this.settings = { ...this.settings, ...JSON.parse(saved) };
        }
        
        const badgeCount = localStorage.getItem('badgeCount');
        if (badgeCount) {
            this.badgeCount = parseInt(badgeCount);
        }
    }

    /**
     * Save settings to localStorage
     */
    saveSettings() {
        localStorage.setItem('notificationSettings', JSON.stringify(this.settings));
        localStorage.setItem('badgeCount', this.badgeCount.toString());
    }

    /**
     * Request notification permission
     */
    async requestPermission() {
        if (!this.checkSupport()) return false;

        try {
            const permission = await Notification.requestPermission();
            this.settings.permission = permission;
            this.saveSettings();
            
            if (permission === 'granted') {
                console.log('[NotificationManager] Permission granted');
                this.sendWelcomeNotification();
                return true;
            } else {
                console.warn('[NotificationManager] Permission denied');
                return false;
            }
        } catch (error) {
            console.error('[NotificationManager] Error requesting permission:', error);
            return false;
        }
    }

    /**
     * Check if permission is granted
     */
    hasPermission() {
        return Notification.permission === 'granted';
    }

    /**
     * Send welcome notification
     */
    sendWelcomeNotification() {
        this.send({
            title: '🎉 Notifications activées',
            body: 'Vous recevrez maintenant des alertes en temps réel',
            tag: 'welcome',
            silent: true
        });
    }

    /**
     * Play notification sound
     */
    playSound(type = 'default') {
        if (!this.settings.sound) return;

        try {
            const audioContext = new (window.AudioContext || window.webkitAudioContext)();
            const oscillator = audioContext.createOscillator();
            const gainNode = audioContext.createGain();
            
            oscillator.connect(gainNode);
            gainNode.connect(audioContext.destination);
            
            // Different frequencies for different alert types
            const frequencies = {
                motion: 440,      // A4
                intrusion: 880,   // A5
                critical: 1046.5, // C6
                info: 523.25,     // C5
                default: 440
            };
            
            oscillator.frequency.value = frequencies[type] || frequencies.default;
            oscillator.type = 'sine';
            
            gainNode.gain.setValueAtTime(0.3, audioContext.currentTime);
            gainNode.gain.exponentialRampToValueAtTime(0.01, audioContext.currentTime + 0.5);
            
            oscillator.start(audioContext.currentTime);
            oscillator.stop(audioContext.currentTime + 0.5);
        } catch (error) {
            console.error('[NotificationManager] Error playing sound:', error);
        }
    }

    /**
     * Vibrate device
     */
    vibrate(pattern = [200]) {
        if (!this.settings.vibration) return;
        
        if ('vibrate' in navigator) {
            navigator.vibrate(pattern);
        }
    }

    /**
     * Update badge count
     */
    updateBadge(count) {
        if (!this.settings.badge) return;
        
        this.badgeCount = count;
        this.saveSettings();
        
        if ('setAppBadge' in navigator) {
            navigator.setAppBadge(count);
        }
    }

    /**
     * Increment badge count
     */
    incrementBadge() {
        this.updateBadge(this.badgeCount + 1);
    }

    /**
     * Clear badge
     */
    clearBadge() {
        this.updateBadge(0);
        if ('clearAppBadge' in navigator) {
            navigator.clearAppBadge();
        }
    }

    /**
     * Send notification
     * @param {Object} options - Notification options
     */
    send(options) {
        if (!this.hasPermission()) {
            console.warn('[NotificationManager] Permission not granted');
            return null;
        }

        const defaults = {
            icon: '🔔',
            badge: '🔔',
            requireInteraction: false,
            silent: !this.settings.sound,
            data: {
                timestamp: Date.now()
            }
        };

        const config = { ...defaults, ...options };

        // Play sound
        if (config.soundType) {
            this.playSound(config.soundType);
        }

        // Vibrate
        if (config.vibrate) {
            this.vibrate(config.vibrate);
        }

        // Create notification
        const notification = new Notification(config.title, config);

        // Handle click
        notification.onclick = (event) => {
            event.preventDefault();
            window.focus();
            notification.close();
            
            if (config.onClick) {
                config.onClick(event);
            }
        };

        // Update badge
        if (!config.silent) {
            this.incrementBadge();
        }

        console.log('[NotificationManager] Notification sent:', config.title);
        return notification;
    }

    /**
     * Send motion detection notification
     */
    sendMotionAlert(cameraId, cameraName) {
        return this.send({
            title: '🎥 Détection de Mouvement',
            body: `Mouvement détecté sur ${cameraName || cameraId}`,
            icon: '🎥',
            tag: `motion-${cameraId}`,
            soundType: 'motion',
            vibrate: [200, 100, 200],
            data: {
                type: 'motion',
                cameraId: cameraId,
                cameraName: cameraName
            },
            onClick: (event) => {
                console.log(`[NotificationManager] Motion alert clicked for ${cameraId}`);
                // Navigate to camera
                window.location.href = `/#camera-${cameraId}`;
            }
        });
    }

    /**
     * Send intrusion alert notification
     */
    sendIntrusionAlert(cameraId, cameraName) {
        return this.send({
            title: '⚠️ Alerte Intrusion',
            body: `Intrusion potentielle détectée sur ${cameraName || cameraId}`,
            icon: '⚠️',
            tag: `intrusion-${cameraId}`,
            soundType: 'intrusion',
            vibrate: [300, 100, 300, 100, 300],
            requireInteraction: true,
            data: {
                type: 'intrusion',
                cameraId: cameraId,
                cameraName: cameraName
            },
            onClick: (event) => {
                console.log(`[NotificationManager] Intrusion alert clicked for ${cameraId}`);
                window.location.href = `/#camera-${cameraId}`;
            }
        });
    }

    /**
     * Send critical alert notification
     */
    sendCriticalAlert(message, data = {}) {
        return this.send({
            title: '🚨 ALERTE CRITIQUE',
            body: message,
            icon: '🚨',
            tag: 'critical-alert',
            soundType: 'critical',
            vibrate: [500, 200, 500, 200, 500],
            requireInteraction: true,
            data: {
                type: 'critical',
                ...data
            },
            onClick: (event) => {
                console.log('[NotificationManager] Critical alert clicked');
                window.location.href = '/';
            }
        });
    }

    /**
     * Send info notification
     */
    sendInfo(title, message) {
        return this.send({
            title: `ℹ️ ${title}`,
            body: message,
            icon: 'ℹ️',
            tag: 'info',
            soundType: 'info',
            vibrate: [100],
            silent: true,
            data: {
                type: 'info'
            }
        });
    }

    /**
     * Toggle setting
     */
    toggleSetting(setting) {
        if (setting in this.settings) {
            this.settings[setting] = !this.settings[setting];
            this.saveSettings();
            return this.settings[setting];
        }
        return false;
    }

    /**
     * Get current settings
     */
    getSettings() {
        return { ...this.settings };
    }
}

// Export for use in modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = NotificationManager;
}

// Create global instance
if (typeof window !== 'undefined') {
    window.NotificationManager = NotificationManager;
    window.notificationManager = new NotificationManager();
}
