// tiempo: time.Now, time.Sleep, cronómetro y temporizador.
//
// El reloj monótono de SaguiOS tiene una resolución de 1 ms (SIS_TEMPO).
package main

import (
	"fmt"
	"time"
)

func main() {
	// La hora actual (UTC) y el año: SaguiOS lee el reloj de la placa.
	ahora := time.Now().UTC()
	fmt.Println("año:", ahora.Year() >= 2024)
	fmt.Println("formato:", ahora.Format("2006-01-02") == fmt.Sprintf("%04d-%02d-%02d", ahora.Year(), int(ahora.Month()), ahora.Day()))

	// Cronómetro: time.Since mide lo que tarda un Sleep.
	inicio := time.Now()
	time.Sleep(120 * time.Millisecond)
	t := time.Since(inicio)
	fmt.Println("dormir 120 ms midió entre 100 y 400 ms:", t >= 100*time.Millisecond && t < 400*time.Millisecond)

	// Ticker: cuatro «tics» cada 50 ms.
	tic := time.NewTicker(50 * time.Millisecond)
	n := 0
	for range tic.C {
		n++
		fmt.Println("tic", n)
		if n == 4 {
			tic.Stop()
			break
		}
	}

	// Temporizador: se dispara una vez (y otro que se cancela).
	temp := time.NewTimer(80 * time.Millisecond)
	cancelado := time.NewTimer(10 * time.Second)
	fmt.Println("cancelado.Stop() =", cancelado.Stop())
	<-temp.C
	fmt.Println("el temporizador de 80 ms se disparó")

	// Duraciones y aritmética de fechas.
	d := 90*time.Minute + 30*time.Second
	fmt.Println("duración:", d, "horas:", d.Hours())
	f := time.Date(2026, time.November, 16, 12, 0, 0, 0, time.UTC)
	fmt.Println("dentro de 100 días:", f.AddDate(0, 0, 100).Format("2006-01-02"), f.AddDate(0, 0, 100).Weekday())
	fmt.Println("tiempo: terminado")
}
