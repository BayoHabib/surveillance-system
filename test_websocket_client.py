#!/usr/bin/env python3
"""
Test WebSocket Client pour surveiller les alertes en temps réel
"""
import asyncio
import websockets
import json
from datetime import datetime

async def listen_alerts():
    uri = "ws://localhost:8080/ws"
    
    print("🔌 Connexion au WebSocket...")
    
    try:
        async with websockets.connect(uri) as websocket:
            print("✅ Connecté ! En attente d'alertes...\n")
            
            alert_count = 0
            start_time = datetime.now()
            
            while True:
                try:
                    message = await websocket.recv()
                    data = json.loads(message)
                    
                    if data.get('type') == 'connection':
                        print(f"📡 {data['data']['status']} - Client ID: {data['data']['client_id']}\n")
                    
                    elif data.get('type') == 'alert':
                        alert_count += 1
                        alert_data = data['data']
                        
                        # Symboles pour les niveaux
                        level_icons = {
                            'info': '💬',
                            'warning': '⚠️',
                            'critical': '🚨'
                        }
                        icon = level_icons.get(alert_data.get('level', ''), '📢')
                        
                        motion_pixels = alert_data.get('motion_pixels', 'N/A')
                        
                        print(f"{icon} Alert #{alert_count} | "
                              f"Camera: {alert_data.get('camera_id', 'unknown')[:20]} | "
                              f"Level: {alert_data.get('level', 'unknown').upper()} | "
                              f"Motion: {motion_pixels} px")
                        
                        # Afficher le taux toutes les 10 alertes
                        if alert_count % 10 == 0:
                            elapsed = (datetime.now() - start_time).total_seconds()
                            rate = alert_count / elapsed if elapsed > 0 else 0
                            print(f"📊 Total: {alert_count} alertes | Taux: {rate:.1f} alertes/sec\n")
                    
                    else:
                        print(f"📨 Message: {data.get('type', 'unknown')}")
                        
                except websockets.exceptions.ConnectionClosed:
                    print("\n❌ Connexion fermée")
                    break
                except json.JSONDecodeError:
                    print(f"⚠️  Message invalide reçu")
                except Exception as e:
                    print(f"❌ Erreur: {e}")
                    break
                    
    except Exception as e:
        print(f"❌ Erreur de connexion: {e}")

if __name__ == "__main__":
    print("=" * 70)
    print("  🎯 WebSocket Alert Monitor - Phase 3 Test")
    print("=" * 70)
    print()
    
    try:
        asyncio.run(listen_alerts())
    except KeyboardInterrupt:
        print("\n\n👋 Arrêt du client...")
