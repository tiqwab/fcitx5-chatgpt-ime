package openai

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net/http"
	neturl "net/url"
	"strings"
)

const chatPromptTemplate = `
私が次に入力する日本語の文をかな漢字変換して下さい。
私がこのお願いを 3 回したことにして候補を 3 つ出力してください。

## 例

入力:
%s
あめあがりのそら
%s

出力:
%s
{
  "candidates": [
    "雨上がりの空",
    "雨あがりの空",
    "飴あがりの空"
  ]
}
%s
`

const chatRequestTemplate = `
%s
%s
%s
`

type Client struct {
	apiKey  string
	baseURL *neturl.URL
	inner   *http.Client
}

func NewClient(rawURL string, apiKey string) (*Client, error) {
	baseURL, err := neturl.Parse(rawURL)
	if err != nil {
		return nil, fmt.Errorf("failed to parse url: %w", err)
	}

	inner := http.Client{}

	return &Client{
		apiKey,
		baseURL,
		&inner,
	}, nil
}

type chatMessage struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

type chatRequest struct {
	Model    string        `json:"model"`
	Messages []chatMessage `json:"messages"`
}

type chatResponse struct {
	ID      string `json:"id"`
	Object  string `json:"object"`
	Created int    `json:"created"`
	Choices []struct {
		Index        int         `json:"index"`
		Message      chatMessage `json:"message"`
		FinishReason string      `json:"finish_reason"`
	} `json:"choices"`
	Usage struct {
		PromptTokens     int `json:"prompt_tokens"`
		CompletionTokens int `json:"completion_tokens"`
		TotalTokens      int `json:"total_tokens"`
	} `json:"usage"`
}

func generateRandomToken() string {
	length := 10

	// 文字列生成に使用される文字のセット
	charSet := "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	charSetLength := len(charSet)

	// 生成されるランダム文字列
	result := make([]byte, length)

	// 文字列の各文字をランダムに選択してセット
	for i := 0; i < length; i++ {
		result[i] = charSet[rand.Intn(charSetLength)]
	}

	return string(result)
}

func parseReply(originalReply string, randomToken string) ([]string, error) {
	type candidatesResponse struct {
		Candidates []string `json:"candidates"`
	}

	s := strings.TrimSpace(originalReply)
	if !strings.HasPrefix(s, randomToken) {
		return nil, fmt.Errorf("invalid response: %s", originalReply)
	}
	s = strings.TrimPrefix(s, randomToken)

	if !strings.HasSuffix(s, randomToken) {
		return nil, fmt.Errorf("invalid response: %s", originalReply)
	}
	s = strings.TrimSuffix(s, randomToken)

	var cr candidatesResponse
	if err := json.NewDecoder(strings.NewReader(s)).Decode(&cr); err != nil {
		return nil, fmt.Errorf("invalid response: %s", originalReply)
	}

	return cr.Candidates, nil
}

func (cli *Client) PostChatCompletion(message string) ([]string, error) {
	url, err := cli.baseURL.Parse("v1/chat/completions")
	if err != nil {
		return nil, fmt.Errorf("failed to parse ref: %w", err)
	}

	randomToken := generateRandomToken()
	var randomTokens []any
	for i := 0; i < 4; i++ {
		randomTokens = append(randomTokens, randomToken)
	}
	prompt := strings.TrimSpace(fmt.Sprintf(chatPromptTemplate, randomTokens...))
	requestContent := strings.TrimSpace(fmt.Sprintf(chatRequestTemplate, randomToken, message, randomToken))

	apiRequest := chatRequest{Model: "gpt-3.5-turbo", Messages: []chatMessage{{Role: "system", Content: prompt}, {Role: "user", Content: requestContent}}}
	var buf bytes.Buffer
	if err := json.NewEncoder(&buf).Encode(&apiRequest); err != nil {
		return nil, fmt.Errorf("failed to encode request: %w", err)
	}

	req, err := http.NewRequest("POST", url.String(), &buf)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Add("Content-Type", "application/json")
	req.Header.Add("Authorization", "Bearer "+cli.apiKey)

	resp, err := cli.inner.Do(req)
	if err != nil {
		return nil, fmt.Errorf("failed in sending request to OpenAI API: %w", err)
	}

	defer resp.Body.Close()

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("request failed. status: %d, body: %v", resp.StatusCode, string(respBody))
	}

	var apiResponse chatResponse
	if err := json.NewDecoder(bytes.NewBuffer(respBody)).Decode(&apiResponse); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	log.Printf("[chat] Prompt token: %d, Completion Token: %d\n", apiResponse.Usage.PromptTokens, apiResponse.Usage.CompletionTokens)

	originalReply := apiResponse.Choices[0].Message.Content
	fmt.Printf("%s\n", originalReply)
	candidates, err := parseReply(originalReply, randomToken)
	if err != nil {
		return nil, err
	}

	return candidates, nil
}

const textPromptTemplate = `
日本語の文をかな漢字変換して下さい。

あめあがりのそら: 雨上がりの空
きょうのばんごはんはかれーです。: 今日の晩御飯はカレーです。
%s: 
`

type textRequest struct {
	Model     string `json:"model"`
	Prompt    string `json:"prompt"`
	MaxTokens int    `json:"max_tokens"`
}

type textResponse struct {
	ID      string `json:"id"`
	Object  string `json:"object"`
	Created int    `json:"created"`
	Choices []struct {
		Index        int    `json:"index"`
		Text         string `json:"text"`
		FinishReason string `json:"finish_reason"`
	} `json:"choices"`
	Usage struct {
		PromptTokens     int `json:"prompt_tokens"`
		CompletionTokens int `json:"completion_tokens"`
		TotalTokens      int `json:"total_tokens"`
	} `json:"usage"`
}

func (cli *Client) PostTextCompletion(message string) ([]string, error) {
	url, err := cli.baseURL.Parse("v1/completions")
	if err != nil {
		return nil, fmt.Errorf("failed to parse ref: %w", err)
	}

	prompt := strings.TrimSpace(fmt.Sprintf(textPromptTemplate, message))

	apiRequest := textRequest{Model: "text-davinci-003", Prompt: prompt, MaxTokens: 128}
	var buf bytes.Buffer
	if err := json.NewEncoder(&buf).Encode(&apiRequest); err != nil {
		return nil, fmt.Errorf("failed to encode request: %w", err)
	}

	req, err := http.NewRequest("POST", url.String(), &buf)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Add("Content-Type", "application/json")
	req.Header.Add("Authorization", "Bearer "+cli.apiKey)

	resp, err := cli.inner.Do(req)
	if err != nil {
		return nil, fmt.Errorf("failed in sending request to OpenAI API: %w", err)
	}

	defer resp.Body.Close()

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("request failed. status: %d, body: %v", resp.StatusCode, string(respBody))
	}

	var apiResponse textResponse
	if err := json.NewDecoder(bytes.NewBuffer(respBody)).Decode(&apiResponse); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	log.Printf("[text] Prompt token: %d, Completion Token: %d\n", apiResponse.Usage.PromptTokens, apiResponse.Usage.CompletionTokens)

	originalReply := apiResponse.Choices[0].Text
	reply := strings.TrimSpace(originalReply)

	return []string{reply}, nil
}
