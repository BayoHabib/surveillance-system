// internal/websocket/hub.go
package websocket

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		return true // Permettre toutes les origines pour le dev
	},
}

type Hub struct {
	clients    map[*Client]bool
	broadcast  chan Message
	register   chan *Client
	unregister chan *Client
	mutex      sync.RWMutex
}

type Client struct {
	hub  *Hub
	conn *websocket.Conn
	send chan Message
	id   string
}

type Message struct {
	Type      string      `json:"type"`
	Data      interface{} `json:"data"`
	Timestamp time.Time   `json:"timestamp"`
}

type Handler struct {
	hub *Hub
}

func NewHub() *Hub {
	return &Hub{
		clients:    make(map[*Client]bool),
		broadcast:  make(chan Message, 256),
		register:   make(chan *Client),
		unregister: make(chan *Client),
	}
}

func NewHandler(hub *Hub) *Handler {
	return &Handler{hub: hub}
}

func (h *Hub) Run() {
	for {
		select {
		case client := <-h.register:
			h.mutex.Lock()
			h.clients[client] = true
			h.mutex.Unlock()
			
			log.Printf("Client connecté: %s", client.id)
			
			// Envoyer message de bienvenue
			welcome := Message{
				Type:      "connection",
				Data:      map[string]string{"status": "connected", "client_id": client.id},
				Timestamp: time.Now(),
			}
			select {
			case client.send <- welcome:
			default:
				h.removeClient(client)
			}

		case client := <-h.unregister:
			h.removeClient(client)

		case message := <-h.broadcast:
			message.Timestamp = time.Now()
			h.mutex.RLock()
			for client := range h.clients {
				select {
				case client.send <- message:
				default:
					h.mutex.RUnlock()
					h.removeClient(client)
					h.mutex.RLock()
				}
			}
			h.mutex.RUnlock()
		}
	}
}

func (h *Hub) removeClient(client *Client) {
	h.mutex.Lock()
	defer h.mutex.Unlock()
	
	if _, ok := h.clients[client]; ok {
		delete(h.clients, client)
		close(client.send)
		client.conn.Close()
		log.Printf("Client déconnecté: %s", client.id)
	}
}

func (h *Hub) Broadcast(message Message) {
	select {
	case h.broadcast <- message:
	default:
		log.Println("Canal broadcast plein, message abandonné")
	}
}

func (h *Hub) GetClientCount() int {
	h.mutex.RLock()
	defer h.mutex.RUnlock()
	return len(h.clients)
}

func (handler *Handler) HandleWebSocket(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("Erreur upgrade WebSocket: %v", err)
		return
	}

	clientID := r.URL.Query().Get("client_id")
	if clientID == "" {
		clientID = generateClientID()
	}

	client := &Client{
		hub:  handler.hub,
		conn: conn,
		send: make(chan Message, 256),
		id:   clientID,
	}

	client.hub.register <- client

	// Démarrer les goroutines de lecture/écriture
	go client.writePump()
	go client.readPump()
}

const (
	writeWait      = 10 * time.Second
	pongWait       = 60 * time.Second
	pingPeriod     = (pongWait * 9) / 10
	maxMessageSize = 512
)

func (c *Client) readPump() {
	defer func() {
		c.hub.unregister <- c
	}()

	c.conn.SetReadLimit(maxMessageSize)
	c.conn.SetReadDeadline(time.Now().Add(pongWait))
	c.conn.SetPongHandler(func(string) error {
		c.conn.SetReadDeadline(time.Now().Add(pongWait))
		return nil
	})

	for {
		_, message, err := c.conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				log.Printf("Erreur WebSocket: %v", err)
			}
			break
		}

		// Traitement des messages entrants du client
		var msg Message
		if err := json.Unmarshal(message, &msg); err != nil {
			log.Printf("Erreur parsing message: %v", err)
			continue
		}

		// Echo pour test ou traitement spécifique
		log.Printf("Message reçu de %s: %s", c.id, msg.Type)
	}
}

func (c *Client) writePump() {
	ticker := time.NewTicker(pingPeriod)
	defer func() {
		ticker.Stop()
		c.conn.Close()
	}()

	for {
		select {
		case message, ok := <-c.send:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if !ok {
				c.conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}

			data, err := json.Marshal(message)
			if err != nil {
				log.Printf("Erreur marshaling message: %v", err)
				continue
			}

			if err := c.conn.WriteMessage(websocket.TextMessage, data); err != nil {
				return
			}

		case <-ticker.C:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				return
			}
		}
	}
}

func generateClientID() string {
	return fmt.Sprintf("client_%d", time.Now().UnixNano())
}
