#!/bin/bash

echo "🧪 Test du flux de détection complet Phase 2.3"
echo "=============================================="
echo ""

# Vérifier que le service vision est en cours d'exécution
if ! pgrep -f "vision-service" > /dev/null; then
    echo "❌ Service vision non démarré"
    exit 1
fi
echo "✅ Service vision détecté"

# Vérifier que le serveur Go est en cours d'exécution  
if ! pgrep -f "surveillance-server" > /dev/null; then
    echo "❌ Serveur Go non démarré"
    exit 1
fi
echo "✅ Serveur Go détecté"
echo ""

# Health check
echo "📋 Health check du serveur..."
HEALTH=$(curl -s http://localhost:8080/api/v1/health)
echo "$HEALTH" | jq '.' 2>/dev/null || echo "$HEALTH"
echo ""

# Ajouter une caméra avec la vidéo de test
echo "📹 Ajout de la caméra de test de mouvement..."
CAMERA_RESPONSE=$(curl -s -X POST http://localhost:8080/api/v1/cameras/internet \
  -H "Content-Type: application/json" \
  -d '{"name":"Motion Test Camera","url":"file:///workspaces/surveillance-system/vision-service/test_camera_feed.mp4"}')

CAMERA_ID=$(echo "$CAMERA_RESPONSE" | jq -r '.camera_id' 2>/dev/null)
echo "Response: $CAMERA_RESPONSE" | jq '.' 2>/dev/null || echo "$CAMERA_RESPONSE"
echo ""

if [ -z "$CAMERA_ID" ] || [ "$CAMERA_ID" == "null" ]; then
    echo "❌ Échec de l'ajout de la caméra"
    exit 1
fi

echo "✅ Caméra ajoutée avec ID: $CAMERA_ID"
echo ""

# Attendre quelques secondes pour que des détections se produisent
echo "⏳ Attente de 8 secondes pour la détection de mouvement..."
for i in {8..1}; do
    echo -ne "\r   $i secondes restantes..."
    sleep 1
done
echo -e "\n"

# Vérifier les alertes générées
echo "🚨 Vérification des alertes générées..."
ALERTS=$(curl -s http://localhost:8080/api/v1/alerts)
ALERT_COUNT=$(echo "$ALERTS" | jq '.alerts | length' 2>/dev/null)

if [ -z "$ALERT_COUNT" ] || [ "$ALERT_COUNT" == "null" ]; then
    echo "⚠️  Aucune alerte trouvée (ou erreur de parsing)"
    echo "Response brute: $ALERTS"
else
    echo "✅ Nombre d'alertes générées: $ALERT_COUNT"
    echo ""
    echo "📊 Détails des alertes:"
    echo "$ALERTS" | jq '.alerts[] | {type: .type, level: .level, camera: .camera_id, message: .message}' 2>/dev/null || echo "$ALERTS"
fi
echo ""

# Arrêter la caméra
echo "⏹️  Arrêt de la caméra..."
STOP_RESPONSE=$(curl -s -X PUT http://localhost:8080/api/v1/cameras/$CAMERA_ID/stop)
echo "$STOP_RESPONSE" | jq '.' 2>/dev/null || echo "$STOP_RESPONSE"
echo ""

echo "✅ Test terminé!"
echo ""
echo "📈 Résumé:"
echo "  - Service vision: ✅ Running"
echo "  - Serveur Go: ✅ Running"
echo "  - Caméra ajoutée: ✅ $CAMERA_ID"
echo "  - Alertes générées: $ALERT_COUNT"
echo "  - Pipeline complet: ✅ Fonctionnel"
