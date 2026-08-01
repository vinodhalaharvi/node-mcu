package main

import (
	"fmt"
	"io"
	"log"
	"net/http"
)

func main() {
	http.HandleFunc("/readings", func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		fmt.Printf("got: %s\n", body)
		w.WriteHeader(http.StatusOK)
	})
	log.Println("listening on :8080")
	log.Fatal(http.ListenAndServe("0.0.0.0:8080", nil))
}
