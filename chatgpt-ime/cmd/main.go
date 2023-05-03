package main

import (
	"encoding/json"
	"errors"
	"io/fs"
	"log"
	"net"
	"net/http"
	"os"

	"github.com/tiqwab/chatgptime/openai"
)

const (
	openAIBaseURL = "https://api.openai.com/"
	socketPath    = "/tmp/chatgpt-ime.sock"
)

type request struct {
	Message string `json:"message"`
}

type response struct {
	Candidates   []string `json:"candidates"`
	ErrorMessage string   `json:"error_message"`
}

func createOpenAIHandler(conversionFunc func(string) ([]string, error)) func(http.ResponseWriter, *http.Request) {
	return func(w http.ResponseWriter, r *http.Request) {
		method := r.Method
		if method != http.MethodPost {
			handleError(w, http.StatusMethodNotAllowed, "method not allowed")
			return
		}

		contentType := r.Header.Get("Content-Type")
		if contentType != "application/json" {
			handleError(w, http.StatusBadRequest, "unsupported Content-Type")
			return
		}

		var req request
		defer r.Body.Close()
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			handleError(w, http.StatusBadRequest, "invalid request")
			return
		}

		if req.Message == "" {
			handleError(w, http.StatusBadRequest, "message should not be empty")
			return
		}

		log.Printf("Received message: %+v\n", req)

		candidates, err := conversionFunc(req.Message)
		if err != nil {
			log.Printf("Failed in calling OpenAI API: %v\n", err)
			handleError(w, http.StatusInternalServerError, "internal server error")
			return
		}

		w.Header().Add("Content-Type", "application/json")
		resp := response{Candidates: candidates}
		log.Printf("Converted message: %+v\n", resp)
		json.NewEncoder(w).Encode(resp)
	}
}

func handleError(w http.ResponseWriter, statusCode int, message string) {
	w.Header().Add("Content-Type", "application/json")
	w.WriteHeader(statusCode)
	resp := response{ErrorMessage: message}
	json.NewEncoder(w).Encode(resp)
}

func healthcheckHandler(w http.ResponseWriter, r *http.Request) {
	defer r.Body.Close()

	if _, err := w.Write([]byte("ok")); err != nil {
		log.Printf("Failed in writing response: %v\n", err)
		handleError(w, http.StatusInternalServerError, "internal server error")
		return
	}
}

func main() {
	apiKey, ok := os.LookupEnv("OPENAI_API_KEY")
	if !ok {
		log.Fatalf("environment variable OPENAI_API_KEY is required.")
	}

	openAIClient, err := openai.NewClient(openAIBaseURL, apiKey)
	if err != nil {
		log.Fatalf("Failed to create OpenAI client: %v\n", err)
	}

	// Remove the socket file if it exists
	if err := os.Remove(socketPath); err != nil && !errors.Is(err, fs.ErrNotExist) {
		log.Fatalf("Failed to remove socket: %v\n", err)
	}

	// Create a Unix domain socket listener
	listener, err := net.Listen("unix", socketPath)
	if err != nil {
		log.Fatalf("Failed to listen on socket: %v\n", err)
	}
	defer listener.Close()

	log.Printf("Listening on %s\n", socketPath)

	// Set up an HTTP server
	http.HandleFunc("/chat", createOpenAIHandler(openAIClient.PostChatCompletion))
	http.HandleFunc("/text", createOpenAIHandler(openAIClient.PostTextCompletion))
	http.HandleFunc("/healthcheck", healthcheckHandler)

	// Serve HTTP over the Unix domain socket
	_ = http.Serve(listener, nil)
}
