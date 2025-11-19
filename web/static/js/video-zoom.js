/**
 * Video Zoom Controller
 * Provides digital zoom and pan capabilities for video elements
 */

class VideoZoomController {
    constructor() {
        this.instances = new Map(); // Map of cameraId -> zoom state
        this.activeCamera = null;
    }

    /**
     * Initialize zoom for a camera video element
     */
    init(cameraId, videoElement) {
        if (!videoElement) {
            console.warn('Video element not found for camera:', cameraId);
            return;
        }

        // Create zoom state
        const state = {
            videoElement,
            container: videoElement.closest('.camera-video-container'),
            zoomLevel: 1.0,
            panX: 0,
            panY: 0,
            isDragging: false,
            dragStartX: 0,
            dragStartY: 0,
            panStartX: 0,
            panStartY: 0
        };

        this.instances.set(cameraId, state);

        // Setup event listeners
        this.setupEventListeners(cameraId, state);

        // Add zoom indicator
        this.createZoomIndicator(cameraId, state);
    }

    /**
     * Setup event listeners for zoom and pan
     */
    setupEventListeners(cameraId, state) {
        const { container, videoElement } = state;

        // Mouse wheel for zoom
        container.addEventListener('wheel', (e) => {
            e.preventDefault();
            this.handleWheel(cameraId, e);
        }, { passive: false });

        // Mouse events for pan
        videoElement.addEventListener('mousedown', (e) => {
            if (state.zoomLevel > 1.0) {
                this.startPan(cameraId, e);
            }
        });

        videoElement.addEventListener('mousemove', (e) => {
            if (state.isDragging) {
                this.updatePan(cameraId, e);
            }
        });

        videoElement.addEventListener('mouseup', () => {
            this.endPan(cameraId);
        });

        videoElement.addEventListener('mouseleave', () => {
            this.endPan(cameraId);
        });

        // Double-click to reset
        videoElement.addEventListener('dblclick', (e) => {
            e.preventDefault();
            this.reset(cameraId);
        });

        // Update cursor
        videoElement.addEventListener('mousemove', () => {
            this.updateCursor(cameraId);
        });

        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (this.activeCamera === cameraId) {
                this.handleKeyboard(cameraId, e);
            }
        });

        // Track active camera on hover
        container.addEventListener('mouseenter', () => {
            this.activeCamera = cameraId;
        });

        container.addEventListener('mouseleave', () => {
            if (this.activeCamera === cameraId) {
                this.activeCamera = null;
            }
        });
    }

    /**
     * Handle mouse wheel zoom
     */
    handleWheel(cameraId, event) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        const delta = event.deltaY > 0 ? -0.2 : 0.2;
        const newZoom = Math.max(1.0, Math.min(8.0, state.zoomLevel + delta));

        // Get mouse position relative to video
        const rect = state.videoElement.getBoundingClientRect();
        const mouseX = (event.clientX - rect.left) / rect.width;
        const mouseY = (event.clientY - rect.top) / rect.height;

        // Adjust pan to zoom towards mouse position
        if (newZoom > state.zoomLevel) {
            // Zooming in - move towards mouse
            const zoomRatio = newZoom / state.zoomLevel;
            state.panX = state.panX * zoomRatio + (mouseX - 0.5) * (newZoom - state.zoomLevel) * 100;
            state.panY = state.panY * zoomRatio + (mouseY - 0.5) * (newZoom - state.zoomLevel) * 100;
        } else if (newZoom < state.zoomLevel) {
            // Zooming out - proportional pan
            const zoomRatio = newZoom / state.zoomLevel;
            state.panX *= zoomRatio;
            state.panY *= zoomRatio;
        }

        state.zoomLevel = newZoom;
        this.applyTransform(cameraId);
        this.updateZoomIndicator(cameraId);

        // Show/hide reset button
        this.updateResetButton(cameraId);
    }

    /**
     * Start panning
     */
    startPan(cameraId, event) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        state.isDragging = true;
        state.dragStartX = event.clientX;
        state.dragStartY = event.clientY;
        state.panStartX = state.panX;
        state.panStartY = state.panY;

        state.videoElement.style.cursor = 'grabbing';
    }

    /**
     * Update pan position
     */
    updatePan(cameraId, event) {
        const state = this.instances.get(cameraId);
        if (!state || !state.isDragging) return;

        const rect = state.videoElement.getBoundingClientRect();
        const deltaX = (event.clientX - state.dragStartX) / rect.width * 100;
        const deltaY = (event.clientY - state.dragStartY) / rect.height * 100;

        // Calculate new pan position
        state.panX = state.panStartX + deltaX;
        state.panY = state.panStartY + deltaY;

        // Clamp pan to prevent going out of bounds
        const maxPan = (state.zoomLevel - 1) * 50;
        state.panX = Math.max(-maxPan, Math.min(maxPan, state.panX));
        state.panY = Math.max(-maxPan, Math.min(maxPan, state.panY));

        this.applyTransform(cameraId);
    }

    /**
     * End panning
     */
    endPan(cameraId) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        state.isDragging = false;
        this.updateCursor(cameraId);
    }

    /**
     * Handle keyboard shortcuts
     */
    handleKeyboard(cameraId, event) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        switch(event.key) {
            case '+':
            case '=':
                event.preventDefault();
                this.zoomIn(cameraId);
                break;
            case '-':
                event.preventDefault();
                this.zoomOut(cameraId);
                break;
            case '0':
                event.preventDefault();
                this.reset(cameraId);
                break;
            case 'ArrowUp':
                if (state.zoomLevel > 1.0) {
                    event.preventDefault();
                    state.panY -= 5;
                    this.applyTransform(cameraId);
                }
                break;
            case 'ArrowDown':
                if (state.zoomLevel > 1.0) {
                    event.preventDefault();
                    state.panY += 5;
                    this.applyTransform(cameraId);
                }
                break;
            case 'ArrowLeft':
                if (state.zoomLevel > 1.0) {
                    event.preventDefault();
                    state.panX -= 5;
                    this.applyTransform(cameraId);
                }
                break;
            case 'ArrowRight':
                if (state.zoomLevel > 1.0) {
                    event.preventDefault();
                    state.panX += 5;
                    this.applyTransform(cameraId);
                }
                break;
        }
    }

    /**
     * Zoom in
     */
    zoomIn(cameraId) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        state.zoomLevel = Math.min(8.0, state.zoomLevel + 0.5);
        this.applyTransform(cameraId);
        this.updateZoomIndicator(cameraId);
        this.updateResetButton(cameraId);
    }

    /**
     * Zoom out
     */
    zoomOut(cameraId) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        const oldZoom = state.zoomLevel;
        state.zoomLevel = Math.max(1.0, state.zoomLevel - 0.5);

        // Proportionally reduce pan
        if (state.zoomLevel < oldZoom) {
            const ratio = state.zoomLevel / oldZoom;
            state.panX *= ratio;
            state.panY *= ratio;
        }

        this.applyTransform(cameraId);
        this.updateZoomIndicator(cameraId);
        this.updateResetButton(cameraId);
    }

    /**
     * Reset zoom and pan
     */
    reset(cameraId) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        state.zoomLevel = 1.0;
        state.panX = 0;
        state.panY = 0;

        this.applyTransform(cameraId);
        this.updateZoomIndicator(cameraId);
        this.updateResetButton(cameraId);

        Toast.info('Zoom réinitialisé');
    }

    /**
     * Apply transform to video element
     */
    applyTransform(cameraId) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        const { videoElement, zoomLevel, panX, panY } = state;
        
        videoElement.style.transform = `scale(${zoomLevel}) translate(${panX}px, ${panY}px)`;
        videoElement.style.transition = state.isDragging ? 'none' : 'transform 0.2s ease-out';
    }

    /**
     * Update cursor based on zoom level
     */
    updateCursor(cameraId) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        if (state.isDragging) {
            state.videoElement.style.cursor = 'grabbing';
        } else if (state.zoomLevel > 1.0) {
            state.videoElement.style.cursor = 'grab';
        } else {
            state.videoElement.style.cursor = 'default';
        }
    }

    /**
     * Create zoom indicator UI
     */
    createZoomIndicator(cameraId, state) {
        const indicator = document.createElement('div');
        indicator.className = 'zoom-indicator';
        indicator.id = `zoom-indicator-${cameraId}`;
        indicator.innerHTML = '1.0×';
        indicator.style.display = 'none';

        state.container.appendChild(indicator);

        // Create reset button
        const resetBtn = document.createElement('button');
        resetBtn.className = 'zoom-reset-btn';
        resetBtn.id = `zoom-reset-${cameraId}`;
        resetBtn.innerHTML = '⟲';
        resetBtn.title = 'Réinitialiser le zoom (Double-clic ou 0)';
        resetBtn.style.display = 'none';
        resetBtn.onclick = () => this.reset(cameraId);

        state.container.appendChild(resetBtn);

        // Create zoom controls
        const controls = document.createElement('div');
        controls.className = 'zoom-controls';
        controls.id = `zoom-controls-${cameraId}`;
        controls.innerHTML = `
            <button class="zoom-btn" onclick="videoZoom.zoomIn('${cameraId}')" title="Zoom avant (+)">+</button>
            <button class="zoom-btn" onclick="videoZoom.zoomOut('${cameraId}')" title="Zoom arrière (-)">−</button>
        `;

        state.container.appendChild(controls);
    }

    /**
     * Update zoom indicator text
     */
    updateZoomIndicator(cameraId) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        const indicator = document.getElementById(`zoom-indicator-${cameraId}`);
        if (!indicator) return;

        indicator.textContent = `${state.zoomLevel.toFixed(1)}×`;
        indicator.style.display = state.zoomLevel > 1.0 ? 'flex' : 'none';
    }

    /**
     * Update reset button visibility
     */
    updateResetButton(cameraId) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        const resetBtn = document.getElementById(`zoom-reset-${cameraId}`);
        if (!resetBtn) return;

        resetBtn.style.display = state.zoomLevel > 1.0 ? 'flex' : 'none';
    }

    /**
     * Get current zoom level
     */
    getZoomLevel(cameraId) {
        const state = this.instances.get(cameraId);
        return state ? state.zoomLevel : 1.0;
    }

    /**
     * Set zoom level programmatically
     */
    setZoomLevel(cameraId, level) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        state.zoomLevel = Math.max(1.0, Math.min(8.0, level));
        state.panX = 0;
        state.panY = 0;

        this.applyTransform(cameraId);
        this.updateZoomIndicator(cameraId);
        this.updateResetButton(cameraId);
    }

    /**
     * Zoom to specific region
     */
    zoomToRegion(cameraId, x, y, width, height) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        const rect = state.videoElement.getBoundingClientRect();
        
        // Calculate required zoom level to fit region
        const zoomX = rect.width / width;
        const zoomY = rect.height / height;
        const targetZoom = Math.min(zoomX, zoomY, 8.0);

        // Calculate pan to center the region
        const centerX = x + width / 2;
        const centerY = y + height / 2;
        const videoCenterX = rect.width / 2;
        const videoCenterY = rect.height / 2;

        state.zoomLevel = targetZoom;
        state.panX = (videoCenterX - centerX * targetZoom) / targetZoom;
        state.panY = (videoCenterY - centerY * targetZoom) / targetZoom;

        this.applyTransform(cameraId);
        this.updateZoomIndicator(cameraId);
        this.updateResetButton(cameraId);

        Toast.success('Zoom sur la région');
    }

    /**
     * Enable Picture-in-Picture mode
     */
    async togglePiP(cameraId) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        const { videoElement } = state;

        try {
            if (document.pictureInPictureElement) {
                await document.exitPictureInPicture();
                Toast.info('PiP désactivé');
            } else {
                await videoElement.requestPictureInPicture();
                Toast.success('PiP activé');
            }
        } catch (error) {
            console.error('PiP error:', error);
            Toast.error('PiP non disponible');
        }
    }

    /**
     * Clean up zoom instance
     */
    destroy(cameraId) {
        const state = this.instances.get(cameraId);
        if (!state) return;

        // Remove UI elements
        const indicator = document.getElementById(`zoom-indicator-${cameraId}`);
        const resetBtn = document.getElementById(`zoom-reset-${cameraId}`);
        const controls = document.getElementById(`zoom-controls-${cameraId}`);

        indicator?.remove();
        resetBtn?.remove();
        controls?.remove();

        // Reset transform
        state.videoElement.style.transform = '';
        state.videoElement.style.cursor = 'default';

        this.instances.delete(cameraId);
    }
}

// Global instance
window.videoZoom = new VideoZoomController();
