/**
 * Snapshot Editor Module
 * Advanced snapshot capture with annotations, metadata overlay, and export
 */

class SnapshotEditor {
    constructor() {
        this.canvas = null;
        this.ctx = null;
        this.sourceImage = null;
        this.annotations = [];
        this.currentTool = null;
        this.isDrawing = false;
        this.startX = 0;
        this.startY = 0;
        this.currentAnnotation = null;
        
        // Settings
        this.settings = {
            color: '#ff0000',
            strokeWidth: 3,
            fontSize: 24,
            text: '',
            showTimestamp: true,
            showCameraName: true,
            showLogo: false,
            showWatermark: true
        };

        this.cameraInfo = {
            id: '',
            name: '',
            location: ''
        };

        this.init();
    }

    init() {
        this.createModal();
        this.setupEventListeners();
    }

    createModal() {
        const modal = document.createElement('div');
        modal.className = 'snapshot-modal';
        modal.id = 'snapshotModal';
        modal.innerHTML = `
            <div class="snapshot-container">
                <!-- Header -->
                <div class="snapshot-header">
                    <h2 class="snapshot-title">
                        <span class="icon">📸</span>
                        Éditeur de Capture
                    </h2>
                    <button class="snapshot-close" onclick="snapshotEditor.close()">×</button>
                </div>

                <!-- Body -->
                <div class="snapshot-body">
                    <!-- Canvas Area -->
                    <div class="snapshot-canvas-area">
                        <canvas id="snapshotCanvas" class="snapshot-canvas"></canvas>
                        <div class="snapshot-loading" style="display: none;">
                            <div class="spinner"></div>
                            <span>Génération en cours...</span>
                        </div>
                    </div>

                    <!-- Sidebar -->
                    <div class="snapshot-sidebar">
                        <!-- Tools Section -->
                        <div class="snapshot-section">
                            <div class="snapshot-section-title">🛠️ Outils</div>
                            <div class="snapshot-tools">
                                <button class="tool-btn" data-tool="arrow">
                                    <span class="icon">➡️</span>
                                    Flèche
                                </button>
                                <button class="tool-btn" data-tool="rect">
                                    <span class="icon">⬜</span>
                                    Rectangle
                                </button>
                                <button class="tool-btn" data-tool="circle">
                                    <span class="icon">⭕</span>
                                    Cercle
                                </button>
                                <button class="tool-btn" data-tool="text">
                                    <span class="icon">📝</span>
                                    Texte
                                </button>
                                <button class="tool-btn" data-tool="pen">
                                    <span class="icon">✏️</span>
                                    Dessin
                                </button>
                                <button class="tool-btn" data-tool="blur">
                                    <span class="icon">🔒</span>
                                    Flou
                                </button>
                            </div>
                        </div>

                        <!-- Colors Section -->
                        <div class="snapshot-section">
                            <div class="snapshot-section-title">🎨 Couleur</div>
                            <div class="color-picker">
                                <div class="color-option active" data-color="#ff0000" style="background: #ff0000;"></div>
                                <div class="color-option" data-color="#00ff00" style="background: #00ff00;"></div>
                                <div class="color-option" data-color="#0000ff" style="background: #0000ff;"></div>
                                <div class="color-option" data-color="#ffff00" style="background: #ffff00;"></div>
                                <div class="color-option" data-color="#ff00ff" style="background: #ff00ff;"></div>
                                <div class="color-option" data-color="#00ffff" style="background: #00ffff;"></div>
                                <div class="color-option" data-color="#ffffff" style="background: #ffffff;"></div>
                                <div class="color-option" data-color="#000000" style="background: #000000;"></div>
                            </div>

                            <div class="stroke-slider">
                                <label style="font-size: 13px; color: var(--text-secondary);">Épaisseur: <span id="strokeValue">3</span>px</label>
                                <input type="range" min="1" max="10" value="3" id="strokeWidth" style="width: 100%;">
                                <div class="stroke-preview">
                                    <div class="stroke-preview-line" id="strokePreview"></div>
                                </div>
                            </div>
                        </div>

                        <!-- Text Section -->
                        <div class="snapshot-section" id="textSection" style="display: none;">
                            <div class="snapshot-section-title">📝 Texte</div>
                            <div class="text-input-group">
                                <textarea id="annotationText" placeholder="Entrez votre texte..."></textarea>
                            </div>
                            <div class="font-size-slider">
                                <label style="font-size: 13px; color: var(--text-secondary);">Taille: <span id="fontSizeValue">24</span>px</label>
                                <input type="range" min="12" max="72" value="24" id="fontSize" style="width: 100%;">
                                <div class="font-size-preview" id="fontPreview">Aperçu</div>
                            </div>
                        </div>

                        <!-- Metadata Section -->
                        <div class="snapshot-section">
                            <div class="snapshot-section-title">ℹ️ Informations</div>
                            <div class="snapshot-metadata">
                                <div class="metadata-item">
                                    <input type="checkbox" id="showTimestamp" checked>
                                    <label for="showTimestamp">Horodatage</label>
                                </div>
                                <div class="metadata-item">
                                    <input type="checkbox" id="showCameraName" checked>
                                    <label for="showCameraName">Nom caméra</label>
                                </div>
                                <div class="metadata-item">
                                    <input type="checkbox" id="showWatermark" checked>
                                    <label for="showWatermark">Filigrane</label>
                                </div>
                            </div>
                        </div>

                        <!-- Annotations List -->
                        <div class="snapshot-section">
                            <div class="snapshot-section-title">📋 Annotations (<span id="annotationCount">0</span>)</div>
                            <div class="annotations-list" id="annotationsList">
                                <p style="font-size: 13px; color: var(--text-secondary); text-align: center;">Aucune annotation</p>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- Footer -->
                <div class="snapshot-footer">
                    <button class="btn btn-secondary" onclick="snapshotEditor.clear()">
                        🗑️ Effacer tout
                    </button>
                    <button class="btn btn-secondary" onclick="snapshotEditor.undo()">
                        ↶ Annuler
                    </button>
                    <button class="btn btn-primary" onclick="snapshotEditor.download('png')">
                        💾 Télécharger PNG
                    </button>
                    <button class="btn btn-success" onclick="snapshotEditor.download('jpg')">
                        📥 Télécharger JPG
                    </button>
                </div>
            </div>
        `;

        document.body.appendChild(modal);
    }

    setupEventListeners() {
        // Tool buttons
        document.querySelectorAll('.tool-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const tool = e.currentTarget.dataset.tool;
                this.selectTool(tool);
            });
        });

        // Color picker
        document.querySelectorAll('.color-option').forEach(option => {
            option.addEventListener('click', (e) => {
                const color = e.currentTarget.dataset.color;
                this.setColor(color);
            });
        });

        // Stroke width
        const strokeWidth = document.getElementById('strokeWidth');
        if (strokeWidth) {
            strokeWidth.addEventListener('input', (e) => {
                this.settings.strokeWidth = parseInt(e.target.value);
                document.getElementById('strokeValue').textContent = e.target.value;
                document.getElementById('strokePreview').style.height = `${e.target.value}px`;
            });
        }

        // Font size
        const fontSize = document.getElementById('fontSize');
        if (fontSize) {
            fontSize.addEventListener('input', (e) => {
                this.settings.fontSize = parseInt(e.target.value);
                document.getElementById('fontSizeValue').textContent = e.target.value;
                document.getElementById('fontPreview').style.fontSize = `${e.target.value}px`;
            });
        }

        // Text input
        const annotationText = document.getElementById('annotationText');
        if (annotationText) {
            annotationText.addEventListener('input', (e) => {
                this.settings.text = e.target.value;
            });
        }

        // Metadata checkboxes
        ['showTimestamp', 'showCameraName', 'showWatermark'].forEach(id => {
            const checkbox = document.getElementById(id);
            if (checkbox) {
                checkbox.addEventListener('change', (e) => {
                    this.settings[id] = e.target.checked;
                    this.redraw();
                });
            }
        });

        // Close on Escape
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && document.getElementById('snapshotModal').classList.contains('active')) {
                this.close();
            }
        });
    }

    setupCanvasListeners() {
        if (!this.canvas) return;

        this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.canvas.addEventListener('mouseup', (e) => this.onMouseUp(e));
        this.canvas.addEventListener('mouseleave', (e) => this.onMouseUp(e));
    }

    open(videoElement, cameraInfo) {
        this.cameraInfo = cameraInfo;
        
        // Capture video frame
        const tempCanvas = document.createElement('canvas');
        tempCanvas.width = videoElement.videoWidth;
        tempCanvas.height = videoElement.videoHeight;
        const tempCtx = tempCanvas.getContext('2d');
        tempCtx.drawImage(videoElement, 0, 0);

        // Create image from canvas
        this.sourceImage = new Image();
        this.sourceImage.onload = () => {
            this.initCanvas();
            this.redraw();
        };
        this.sourceImage.src = tempCanvas.toDataURL();

        // Show modal
        document.getElementById('snapshotModal').classList.add('active');
    }

    initCanvas() {
        this.canvas = document.getElementById('snapshotCanvas');
        this.ctx = this.canvas.getContext('2d');

        // Set canvas size
        const maxWidth = window.innerWidth * 0.6;
        const maxHeight = window.innerHeight * 0.7;
        const imgRatio = this.sourceImage.width / this.sourceImage.height;
        
        if (this.sourceImage.width > maxWidth) {
            this.canvas.width = maxWidth;
            this.canvas.height = maxWidth / imgRatio;
        } else {
            this.canvas.width = this.sourceImage.width;
            this.canvas.height = this.sourceImage.height;
        }

        if (this.canvas.height > maxHeight) {
            this.canvas.height = maxHeight;
            this.canvas.width = maxHeight * imgRatio;
        }

        this.setupCanvasListeners();
    }

    selectTool(tool) {
        this.currentTool = tool;
        
        // Update UI
        document.querySelectorAll('.tool-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tool === tool);
        });

        // Show/hide text section
        document.getElementById('textSection').style.display = tool === 'text' ? 'block' : 'none';

        // Update cursor
        this.canvas.classList.remove('drawing', 'text-mode', 'arrow-mode');
        if (tool === 'text') this.canvas.classList.add('text-mode');
        else if (tool === 'arrow') this.canvas.classList.add('arrow-mode');
        else this.canvas.classList.add('drawing');
    }

    setColor(color) {
        this.settings.color = color;
        
        // Update UI
        document.querySelectorAll('.color-option').forEach(option => {
            option.classList.toggle('active', option.dataset.color === color);
        });
    }

    onMouseDown(e) {
        if (!this.currentTool) return;

        this.isDrawing = true;
        const rect = this.canvas.getBoundingClientRect();
        this.startX = e.clientX - rect.left;
        this.startY = e.clientY - rect.top;

        if (this.currentTool === 'text') {
            this.addTextAnnotation(this.startX, this.startY);
        } else if (this.currentTool === 'pen') {
            this.currentAnnotation = {
                type: 'pen',
                color: this.settings.color,
                strokeWidth: this.settings.strokeWidth,
                points: [{ x: this.startX, y: this.startY }]
            };
        }
    }

    onMouseMove(e) {
        if (!this.isDrawing || !this.currentTool) return;

        const rect = this.canvas.getBoundingClientRect();
        const currentX = e.clientX - rect.left;
        const currentY = e.clientY - rect.top;

        if (this.currentTool === 'pen') {
            this.currentAnnotation.points.push({ x: currentX, y: currentY });
            this.redraw();
            this.drawPenPreview();
        } else if (this.currentTool !== 'text') {
            this.redraw();
            this.drawShapePreview(currentX, currentY);
        }
    }

    onMouseUp(e) {
        if (!this.isDrawing) return;

        const rect = this.canvas.getBoundingClientRect();
        const endX = e.clientX - rect.left;
        const endY = e.clientY - rect.top;

        if (this.currentTool === 'pen' && this.currentAnnotation) {
            this.annotations.push(this.currentAnnotation);
            this.currentAnnotation = null;
        } else if (this.currentTool === 'arrow') {
            this.addArrowAnnotation(this.startX, this.startY, endX, endY);
        } else if (this.currentTool === 'rect') {
            this.addRectAnnotation(this.startX, this.startY, endX, endY);
        } else if (this.currentTool === 'circle') {
            this.addCircleAnnotation(this.startX, this.startY, endX, endY);
        } else if (this.currentTool === 'blur') {
            this.addBlurAnnotation(this.startX, this.startY, endX, endY);
        }

        this.isDrawing = false;
        this.redraw();
        this.updateAnnotationsList();
    }

    drawShapePreview(currentX, currentY) {
        this.ctx.strokeStyle = this.settings.color;
        this.ctx.lineWidth = this.settings.strokeWidth;
        this.ctx.setLineDash([5, 5]);

        if (this.currentTool === 'arrow') {
            this.drawArrow(this.startX, this.startY, currentX, currentY);
        } else if (this.currentTool === 'rect') {
            this.ctx.strokeRect(this.startX, this.startY, currentX - this.startX, currentY - this.startY);
        } else if (this.currentTool === 'circle') {
            const radius = Math.sqrt(Math.pow(currentX - this.startX, 2) + Math.pow(currentY - this.startY, 2));
            this.ctx.beginPath();
            this.ctx.arc(this.startX, this.startY, radius, 0, Math.PI * 2);
            this.ctx.stroke();
        } else if (this.currentTool === 'blur') {
            this.ctx.fillStyle = 'rgba(0, 0, 0, 0.3)';
            this.ctx.fillRect(this.startX, this.startY, currentX - this.startX, currentY - this.startY);
        }

        this.ctx.setLineDash([]);
    }

    drawPenPreview() {
        if (!this.currentAnnotation || this.currentAnnotation.points.length < 2) return;

        this.ctx.strokeStyle = this.currentAnnotation.color;
        this.ctx.lineWidth = this.currentAnnotation.strokeWidth;
        this.ctx.lineCap = 'round';
        this.ctx.lineJoin = 'round';

        this.ctx.beginPath();
        this.ctx.moveTo(this.currentAnnotation.points[0].x, this.currentAnnotation.points[0].y);
        
        for (let i = 1; i < this.currentAnnotation.points.length; i++) {
            this.ctx.lineTo(this.currentAnnotation.points[i].x, this.currentAnnotation.points[i].y);
        }
        
        this.ctx.stroke();
    }

    addTextAnnotation(x, y) {
        if (!this.settings.text.trim()) {
            Toast.warning('Veuillez entrer du texte');
            return;
        }

        this.annotations.push({
            type: 'text',
            x, y,
            text: this.settings.text,
            color: this.settings.color,
            fontSize: this.settings.fontSize
        });

        this.redraw();
        this.updateAnnotationsList();
        this.isDrawing = false;
    }

    addArrowAnnotation(x1, y1, x2, y2) {
        this.annotations.push({
            type: 'arrow',
            x1, y1, x2, y2,
            color: this.settings.color,
            strokeWidth: this.settings.strokeWidth
        });
    }

    addRectAnnotation(x1, y1, x2, y2) {
        this.annotations.push({
            type: 'rect',
            x: x1,
            y: y1,
            width: x2 - x1,
            height: y2 - y1,
            color: this.settings.color,
            strokeWidth: this.settings.strokeWidth
        });
    }

    addCircleAnnotation(x1, y1, x2, y2) {
        const radius = Math.sqrt(Math.pow(x2 - x1, 2) + Math.pow(y2 - y1, 2));
        this.annotations.push({
            type: 'circle',
            x: x1,
            y: y1,
            radius,
            color: this.settings.color,
            strokeWidth: this.settings.strokeWidth
        });
    }

    addBlurAnnotation(x1, y1, x2, y2) {
        this.annotations.push({
            type: 'blur',
            x: Math.min(x1, x2),
            y: Math.min(y1, y2),
            width: Math.abs(x2 - x1),
            height: Math.abs(y2 - y1)
        });
    }

    drawArrow(x1, y1, x2, y2) {
        const headLength = 15;
        const angle = Math.atan2(y2 - y1, x2 - x1);

        // Line
        this.ctx.beginPath();
        this.ctx.moveTo(x1, y1);
        this.ctx.lineTo(x2, y2);
        this.ctx.stroke();

        // Arrowhead
        this.ctx.beginPath();
        this.ctx.moveTo(x2, y2);
        this.ctx.lineTo(x2 - headLength * Math.cos(angle - Math.PI / 6), y2 - headLength * Math.sin(angle - Math.PI / 6));
        this.ctx.moveTo(x2, y2);
        this.ctx.lineTo(x2 - headLength * Math.cos(angle + Math.PI / 6), y2 - headLength * Math.sin(angle + Math.PI / 6));
        this.ctx.stroke();
    }

    redraw() {
        if (!this.ctx || !this.sourceImage) return;

        // Clear canvas
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

        // Draw source image
        this.ctx.drawImage(this.sourceImage, 0, 0, this.canvas.width, this.canvas.height);

        // Draw all annotations
        this.annotations.forEach(annotation => {
            if (annotation.type === 'text') {
                this.ctx.font = `${annotation.fontSize}px Arial`;
                this.ctx.fillStyle = annotation.color;
                this.ctx.fillText(annotation.text, annotation.x, annotation.y);
            } else if (annotation.type === 'arrow') {
                this.ctx.strokeStyle = annotation.color;
                this.ctx.lineWidth = annotation.strokeWidth;
                this.ctx.setLineDash([]);
                this.drawArrow(annotation.x1, annotation.y1, annotation.x2, annotation.y2);
            } else if (annotation.type === 'rect') {
                this.ctx.strokeStyle = annotation.color;
                this.ctx.lineWidth = annotation.strokeWidth;
                this.ctx.setLineDash([]);
                this.ctx.strokeRect(annotation.x, annotation.y, annotation.width, annotation.height);
            } else if (annotation.type === 'circle') {
                this.ctx.strokeStyle = annotation.color;
                this.ctx.lineWidth = annotation.strokeWidth;
                this.ctx.setLineDash([]);
                this.ctx.beginPath();
                this.ctx.arc(annotation.x, annotation.y, annotation.radius, 0, Math.PI * 2);
                this.ctx.stroke();
            } else if (annotation.type === 'pen') {
                this.ctx.strokeStyle = annotation.color;
                this.ctx.lineWidth = annotation.strokeWidth;
                this.ctx.lineCap = 'round';
                this.ctx.lineJoin = 'round';
                this.ctx.setLineDash([]);
                
                this.ctx.beginPath();
                this.ctx.moveTo(annotation.points[0].x, annotation.points[0].y);
                annotation.points.forEach(point => {
                    this.ctx.lineTo(point.x, point.y);
                });
                this.ctx.stroke();
            } else if (annotation.type === 'blur') {
                // Apply pixelation effect
                const imageData = this.ctx.getImageData(annotation.x, annotation.y, annotation.width, annotation.height);
                const pixelSize = 10;
                
                for (let y = 0; y < annotation.height; y += pixelSize) {
                    for (let x = 0; x < annotation.width; x += pixelSize) {
                        const i = (y * annotation.width + x) * 4;
                        const r = imageData.data[i];
                        const g = imageData.data[i + 1];
                        const b = imageData.data[i + 2];
                        
                        this.ctx.fillStyle = `rgb(${r},${g},${b})`;
                        this.ctx.fillRect(annotation.x + x, annotation.y + y, pixelSize, pixelSize);
                    }
                }
            }
        });

        // Draw metadata overlay
        this.drawMetadata();
    }

    drawMetadata() {
        const padding = 20;
        const lineHeight = 30;
        let y = padding;

        this.ctx.font = '16px Arial';
        this.ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
        this.ctx.strokeStyle = 'white';
        this.ctx.lineWidth = 3;

        if (this.settings.showTimestamp) {
            const timestamp = new Date().toLocaleString('fr-FR');
            this.ctx.strokeText(`📅 ${timestamp}`, padding, y);
            this.ctx.fillText(`📅 ${timestamp}`, padding, y);
            y += lineHeight;
        }

        if (this.settings.showCameraName && this.cameraInfo.name) {
            this.ctx.strokeText(`📹 ${this.cameraInfo.name}`, padding, y);
            this.ctx.fillText(`📹 ${this.cameraInfo.name}`, padding, y);
            y += lineHeight;
        }

        if (this.settings.showWatermark) {
            this.ctx.font = 'bold 14px Arial';
            this.ctx.fillStyle = 'rgba(255, 255, 255, 0.3)';
            this.ctx.textAlign = 'right';
            this.ctx.fillText('SURVEILLANCE SYSTEM', this.canvas.width - padding, this.canvas.height - padding);
            this.ctx.textAlign = 'left';
        }
    }

    updateAnnotationsList() {
        const list = document.getElementById('annotationsList');
        const count = document.getElementById('annotationCount');
        
        count.textContent = this.annotations.length;

        if (this.annotations.length === 0) {
            list.innerHTML = '<p style="font-size: 13px; color: var(--text-secondary); text-align: center;">Aucune annotation</p>';
            return;
        }

        list.innerHTML = this.annotations.map((annotation, index) => {
            const icons = {
                text: '📝',
                arrow: '➡️',
                rect: '⬜',
                circle: '⭕',
                pen: '✏️',
                blur: '🔒'
            };

            return `
                <div class="annotation-item">
                    <div class="annotation-type">
                        <span>${icons[annotation.type]}</span>
                        <span>${annotation.type === 'text' ? annotation.text.substring(0, 20) : annotation.type}</span>
                    </div>
                    <button class="annotation-delete" onclick="snapshotEditor.deleteAnnotation(${index})">
                        🗑️
                    </button>
                </div>
            `;
        }).join('');
    }

    deleteAnnotation(index) {
        this.annotations.splice(index, 1);
        this.redraw();
        this.updateAnnotationsList();
        Toast.info('Annotation supprimée');
    }

    undo() {
        if (this.annotations.length === 0) {
            Toast.warning('Aucune annotation à annuler');
            return;
        }

        this.annotations.pop();
        this.redraw();
        this.updateAnnotationsList();
        Toast.info('Dernière annotation annulée');
    }

    clear() {
        if (this.annotations.length === 0) {
            Toast.warning('Aucune annotation à effacer');
            return;
        }

        if (confirm('Effacer toutes les annotations ?')) {
            this.annotations = [];
            this.redraw();
            this.updateAnnotationsList();
            Toast.success('Annotations effacées');
        }
    }

    download(format = 'png') {
        const loading = document.querySelector('.snapshot-loading');
        loading.style.display = 'flex';

        setTimeout(() => {
            const mimeType = format === 'jpg' ? 'image/jpeg' : 'image/png';
            const quality = format === 'jpg' ? 0.9 : undefined;

            this.canvas.toBlob(blob => {
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = `snapshot-${this.cameraInfo.id}-${Date.now()}.${format}`;
                a.click();
                URL.revokeObjectURL(url);

                loading.style.display = 'none';
                Toast.success(`Capture enregistrée (${format.toUpperCase()})`);
            }, mimeType, quality);
        }, 100);
    }

    close() {
        document.getElementById('snapshotModal').classList.remove('active');
        this.annotations = [];
        this.currentTool = null;
        this.settings.text = '';
        document.getElementById('annotationText').value = '';
        
        // Reset tool selection
        document.querySelectorAll('.tool-btn').forEach(btn => {
            btn.classList.remove('active');
        });
    }
}

// Global instance
window.snapshotEditor = new SnapshotEditor();
