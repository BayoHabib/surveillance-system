/**
 * WebSocket Manager
 * Handles WebSocket connections with auto-reconnect
 */

class WebSocketManager {
    constructor(url) {
        this.url = url;
        this.ws = null;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 2000;
        this.handlers = {
            open: [],
            message: [],
            close: [],
            error: []
        };
        this.autoReconnect = true;
    }

    /**
     * Connect to WebSocket server
     */
    connect() {
        try {
            this.ws = new WebSocket(this.url);

            this.ws.onopen = (event) => {
                console.log('WebSocket connected');
                this.reconnectAttempts = 0;
                this.handlers.open.forEach(handler => handler(event));
            };

            this.ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    this.handlers.message.forEach(handler => handler(data));
                } catch (error) {
                    console.error('Failed to parse message:', error);
                }
            };

            this.ws.onclose = (event) => {
                console.log('WebSocket disconnected');
                this.handlers.close.forEach(handler => handler(event));
                
                if (this.autoReconnect && this.reconnectAttempts < this.maxReconnectAttempts) {
                    this.reconnectAttempts++;
                    console.log(`Reconnecting... (${this.reconnectAttempts}/${this.maxReconnectAttempts})`);
                    setTimeout(() => this.connect(), this.reconnectDelay * this.reconnectAttempts);
                }
            };

            this.ws.onerror = (event) => {
                console.error('WebSocket error:', event);
                this.handlers.error.forEach(handler => handler(event));
            };

        } catch (error) {
            console.error('Failed to create WebSocket:', error);
        }
    }

    /**
     * Send message through WebSocket
     */
    send(data) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(typeof data === 'string' ? data : JSON.stringify(data));
            return true;
        }
        console.warn('WebSocket not connected');
        return false;
    }

    /**
     * Register event handler
     */
    on(event, handler) {
        if (this.handlers[event]) {
            this.handlers[event].push(handler);
        }
    }

    /**
     * Remove event handler
     */
    off(event, handler) {
        if (this.handlers[event]) {
            this.handlers[event] = this.handlers[event].filter(h => h !== handler);
        }
    }

    /**
     * Disconnect WebSocket
     */
    disconnect() {
        this.autoReconnect = false;
        if (this.ws) {
            this.ws.close();
        }
    }

    /**
     * Check if WebSocket is connected
     */
    isConnected() {
        return this.ws && this.ws.readyState === WebSocket.OPEN;
    }
}

// Export for use in other modules
window.WebSocketManager = WebSocketManager;
