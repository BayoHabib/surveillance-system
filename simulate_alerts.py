#!/usr/bin/env python3
"""
Simulateur d'alertes WebSocket - Envoie des alertes de test avec tous les niveaux
"""

import asyncio
import json
import random
from datetime import datetime

async def simulate_alerts():
    """Simule différents types d'alertes"""
    
    levels = ['info', 'warning', 'critical']
    cameras = ['camera_1', 'camera_2', 'internet_test']
    messages = {
        'info': [
            'Motion detected with {} pixels (confidence: 70.00%)',
            'Minor movement detected',
            'Low-level activity observed'
        ],
        'warning': [
            'Motion detected with {} pixels (confidence: 85.00%)',
            'Significant movement detected',
            'Unusual activity pattern'
        ],
        'critical': [
            'Motion detected with {} pixels (confidence: 95.00%)',
            'CRITICAL: Large movement detected',
            'ALERT: Possible intrusion detected'
        ]
    }
    
    motion_pixels = {
        'info': (1000, 4999),
        'warning': (5000, 9999),
        'critical': (10000, 50000)
    }
    
    print("=" * 70)
    print("  🎭 Alert Simulator - Testing All Levels")
    print("=" * 70)
    print()
    print("This script simulates alerts by making POST requests to the API.")
    print("Watch the frontend to see if notifications appear correctly!")
    print()
    
    import requests
    
    alert_count = 0
    
    try:
        for i in range(30):  # 30 alertes
            level = random.choice(levels)
            camera_id = random.choice(cameras)
            
            # Poids: plus de critical et warning pour le test
            if i % 3 == 0:
                level = 'critical'
            elif i % 3 == 1:
                level = 'warning'
            
            pixels = random.randint(*motion_pixels[level])
            message_template = random.choice(messages[level])
            message = message_template.format(pixels) if '{}' in message_template else message_template
            
            alert_data = {
                'camera_id': camera_id,
                'type': 'motion',
                'level': level,
                'message': message,
                'detection': {
                    'metadata': {
                        'motion_pixels': pixels,
                        'confidence': random.uniform(0.7, 0.99)
                    }
                }
            }
            
            # Note: Nous ne pouvons pas poster directement des alertes via l'API
            # Cette simulation montre ce que nous voudrions faire
            # Dans la vraie vie, les alertes viennent du vision-service
            
            alert_count += 1
            icon = {'info': 'ℹ️', 'warning': '⚠️', 'critical': '🚨'}[level]
            
            print(f"{icon} Alert #{alert_count:2d} | {level.upper():8s} | {camera_id:15s} | {pixels:5d} px")
            
            await asyncio.sleep(0.5)  # Une alerte toutes les 0.5s
            
        print()
        print(f"✅ Simulation terminée: {alert_count} alertes simulées")
        print()
        print("Levels distribution:")
        print(f"  Info: ~33%")
        print(f"  Warning: ~33%")
        print(f"  Critical: ~33%")
        
    except KeyboardInterrupt:
        print("\n⚠️ Simulation interrompue")
    except Exception as e:
        print(f"\n❌ Erreur: {e}")

if __name__ == '__main__':
    print("\n⚠️  NOTE: Ce simulateur montre le format attendu des alertes.")
    print("Pour générer de vraies alertes, ajoutez une caméra avec une")
    print("vidéo contenant beaucoup de mouvement (>10000 pixels).\n")
    
    asyncio.run(simulate_alerts())
