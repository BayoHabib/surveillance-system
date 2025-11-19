/**
 * API Client
 * Centralized API communication with error handling
 */

class APIClient {
    constructor(baseURL = '') {
        this.baseURL = baseURL;
        this.defaultHeaders = {
            'Content-Type': 'application/json'
        };
    }

    /**
     * Make API request
     */
    async request(endpoint, options = {}) {
        const url = `${this.baseURL}${endpoint}`;
        const config = {
            headers: { ...this.defaultHeaders, ...options.headers },
            ...options
        };

        try {
            const response = await fetch(url, config);
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            // Handle different content types
            const contentType = response.headers.get('content-type');
            if (contentType && contentType.includes('application/json')) {
                return await response.json();
            }
            return await response.text();

        } catch (error) {
            console.error(`API Error [${endpoint}]:`, error);
            throw error;
        }
    }

    /**
     * GET request
     */
    async get(endpoint, params = {}) {
        const queryString = new URLSearchParams(params).toString();
        const url = queryString ? `${endpoint}?${queryString}` : endpoint;
        return this.request(url, { method: 'GET' });
    }

    /**
     * POST request
     */
    async post(endpoint, data = {}) {
        return this.request(endpoint, {
            method: 'POST',
            body: JSON.stringify(data)
        });
    }

    /**
     * PUT request
     */
    async put(endpoint, data = {}) {
        return this.request(endpoint, {
            method: 'PUT',
            body: JSON.stringify(data)
        });
    }

    /**
     * DELETE request
     */
    async delete(endpoint) {
        return this.request(endpoint, { method: 'DELETE' });
    }

    // === Camera Endpoints ===

    async getCameras() {
        return this.get('/api/v1/cameras');
    }

    async getCamera(id) {
        return this.get(`/api/v1/cameras/${id}`);
    }

    async createCamera(data) {
        return this.post('/api/v1/cameras', data);
    }

    async updateCamera(id, data) {
        return this.put(`/api/v1/cameras/${id}`, data);
    }

    async deleteCamera(id) {
        return this.delete(`/api/v1/cameras/${id}`);
    }

    async startCamera(id) {
        return this.post(`/api/v1/cameras/${id}/start`);
    }

    async stopCamera(id) {
        return this.post(`/api/v1/cameras/${id}/stop`);
    }

    async getCameraSnapshot(id) {
        return this.get(`/api/v1/cameras/${id}/snapshot`);
    }

    // === Alert Endpoints ===

    async getAlerts(params = {}) {
        return this.get('/api/v1/alerts', params);
    }

    async getAlert(id) {
        return this.get(`/api/v1/alerts/${id}`);
    }

    async acknowledgeAlert(id) {
        return this.post(`/api/v1/alerts/${id}/acknowledge`);
    }

    // === Analytics Endpoints ===

    async getStats(params = {}) {
        return this.get('/api/v1/analytics/stats', params);
    }

    async getDetections(params = {}) {
        return this.get('/api/v1/analytics/detections', params);
    }

    async getCameraActivity(params = {}) {
        return this.get('/api/v1/analytics/camera-activity', params);
    }

    // === System Endpoints ===

    async getSystemHealth() {
        return this.get('/api/v1/system/health');
    }

    async getSystemStats() {
        return this.get('/api/v1/system/stats');
    }
}

// Global instance
window.API = new APIClient();
