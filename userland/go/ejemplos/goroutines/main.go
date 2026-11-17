// goroutines: goroutines, canales, sync.WaitGroup y sync.Mutex.
//
// TinyGo las ejecuta en un solo hilo del sistema (planificador cooperativo):
// una goroutine cede el turno al bloquearse en un canal, dormir o esperar.
package main

import (
	"fmt"
	"sync"
	"time"
)

// productor envía los números 1..n por el canal y lo cierra.
func productor(n int, salida chan<- int) {
	for i := 1; i <= n; i++ {
		salida <- i
	}
	close(salida)
}

// consumidor suma lo que llega y avisa de que terminó.
func consumidor(entrada <-chan int, resultado chan<- int) {
	suma := 0
	for v := range entrada {
		suma += v
	}
	resultado <- suma
}

func main() {
	// Productor y consumidor con un canal con búfer.
	canal := make(chan int, 4)
	resultado := make(chan int)
	go productor(100, canal)
	go consumidor(canal, resultado)
	fmt.Println("suma 1..100 =", <-resultado)

	// WaitGroup y Mutex: 8 goroutines incrementan un contador compartido.
	var wg sync.WaitGroup
	var mu sync.Mutex
	contador := 0
	for g := 0; g < 8; g++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()
			for i := 0; i < 1000; i++ {
				mu.Lock()
				contador++
				mu.Unlock()
			}
		}(g)
	}
	wg.Wait()
	fmt.Println("contador =", contador)

	// Trabajadores en paralelo (concurrentes) que cuadran números.
	trabajos := make(chan int)
	cuadrados := make(chan int)
	var wt sync.WaitGroup
	for w := 0; w < 3; w++ {
		wt.Add(1)
		go func() {
			defer wt.Done()
			for n := range trabajos {
				cuadrados <- n * n
			}
		}()
	}
	go func() {
		for i := 1; i <= 10; i++ {
			trabajos <- i
		}
		close(trabajos)
		wt.Wait()
		close(cuadrados)
	}()
	total := 0
	for c := range cuadrados {
		total += c
	}
	fmt.Println("suma de cuadrados 1..10 =", total)

	// select con tiempo límite.
	lento := make(chan string)
	go func() {
		time.Sleep(200 * time.Millisecond)
		lento <- "tarde"
	}()
	select {
	case m := <-lento:
		fmt.Println("llegó", m)
	case <-time.After(50 * time.Millisecond):
		fmt.Println("tiempo agotado (como se esperaba)")
	}

	// sync.Once.
	var una sync.Once
	for i := 0; i < 3; i++ {
		una.Do(func() { fmt.Println("esto se ve una sola vez") })
	}
	fmt.Println("goroutines: terminado")
}
