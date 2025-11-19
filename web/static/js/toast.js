/**
 * Toast Notification System
 * Displays temporary notifications with various severity levels
 */

class ToastManager {
    constructor() {
        this.container = null;
        this.init();
    }

    init() {
        // Create toast container if it doesn't exist
        if (!document.querySelector('.toast-container')) {
            this.container = document.createElement('div');
            this.container.className = 'toast-container';
            document.body.appendChild(this.container);
        } else {
            this.container = document.querySelector('.toast-container');
        }
    }

    /**
     * Show a toast notification
     * @param {string} message - The message to display
     * @param {string} type - Type of toast (success, error, warning, info)
     * @param {number} duration - Duration in milliseconds (0 = no auto-dismiss)
     * @param {string} title - Optional title
     */
    show(message, type = 'info', duration = 5000, title = '') {
        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;

        const icons = {
            success: '✓',
            error: '✕',
            warning: '⚠',
            info: 'ℹ'
        };

        const titles = {
            success: 'Succès',
            error: 'Erreur',
            warning: 'Attention',
            info: 'Information'
        };

        const icon = icons[type] || icons.info;
        const toastTitle = title || titles[type] || titles.info;

        toast.innerHTML = `
            <span class="toast-icon">${icon}</span>
            <div class="toast-content">
                <div class="toast-title">${toastTitle}</div>
                <div class="toast-message">${message}</div>
            </div>
            <button class="toast-close" onclick="this.parentElement.remove()">×</button>
        `;

        this.container.appendChild(toast);

        // Auto-dismiss after duration
        if (duration > 0) {
            setTimeout(() => {
                toast.style.animation = 'slideOut 0.3s ease';
                setTimeout(() => toast.remove(), 300);
            }, duration);
        }

        return toast;
    }

    success(message, title = '', duration = 5000) {
        return this.show(message, 'success', duration, title);
    }

    error(message, title = '', duration = 5000) {
        return this.show(message, 'error', duration, title);
    }

    warning(message, title = '', duration = 5000) {
        return this.show(message, 'warning', duration, title);
    }

    info(message, title = '', duration = 5000) {
        return this.show(message, 'info', duration, title);
    }

    /**
     * Clear all toasts
     */
    clear() {
        if (this.container) {
            this.container.innerHTML = '';
        }
    }
}

// Add slideOut animation
const style = document.createElement('style');
style.textContent = `
    @keyframes slideOut {
        from {
            transform: translateX(0);
            opacity: 1;
        }
        to {
            transform: translateX(400px);
            opacity: 0;
        }
    }
`;
document.head.appendChild(style);

// Global instance
window.Toast = new ToastManager();
