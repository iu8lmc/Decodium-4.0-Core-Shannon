# El presupuesto del CRC

[Italiano](RAPPORTO_BUDGET_CRC.md) · [English](CRC_BUDGET_REPORT.md) · **Español**

> Mediciones sobre el decodificador LDPC(174,91) de FT2 y FT8: por qué ampliar
> la búsqueda no aporta nada, y qué se gana en cambio reforzando la prueba de
> aceptación.
>
> *Autor: **IU8LMC**. Implementación y mediciones realizadas con la asistencia
> de Claude (Anthropic) bajo la dirección del autor.
> GPL-3.0 — 29 de agosto de 2026.*

---

## 00 · En resumen

> El decodificador de FT8 y FT2 no está limitado por cuánto busca, sino por cómo
> acepta. El CRC de 14 bits deja pasar un candidato erróneo cada 16 384: ampliar
> la búsqueda compra candidatos correctos y falsos en la misma proporción, y no
> compensa. Añadiendo a la prueba de aceptación dos bits de estructura del
> mensaje, los indicativos fantasma se reducen a la mitad sin cambiar las
> decodificaciones.

Este informe recoge lo que se ha medido, incluidas **tres ocasiones en las que
la medición desmintió la previsión**. Los desmentidos se recogen íntegros: son
la parte más útil, porque cada uno habría llevado al aire un empeoramiento
presentado como mejora.

Todo es reproducible. El código es header-only bajo GPL-3.0 en
`Detector/fastldpc/`, los bancos de medida en `lab/cpp/`, y cada número aquí
recogido procede de un comando que puede volver a ejecutarse.

---

## 01 · El contexto en tres párrafos

FT8, FT4 y FT2 usan el mismo código corrector de errores: el LDPC(174,91) del
protocolo FT8, con 77 bits de mensaje más 14 de CRC en los 91 bits de
información, y 83 de paridad. FT2 es un modo de ciclo corto, 3,75 segundos,
donde el tiempo de cálculo por ciclo es ajustado.

Conviene separar de entrada cuatro niveles, porque en lo que sigue sólo el
último es obra de este trabajo. La **clase de códigos** LDPC es de Robert
Gallager, 1962, redescubierta por MacKay y Neal en los años noventa. Los
**algoritmos de decodificación** — min-sum normalizado, ordered statistics
decoding — son literatura consolidada, de Chen y Fossorier el primero, de
Fossorier y Lin el segundo. El **código específico** (174,91), es decir esas 83
filas de paridad, y el CRC de 14 bits con polinomio `0x2757` pertenecen al
protocolo FT8, diseñado por Steve Franke K9AN y Joe Taylor K1JT y publicado en
QEX. El **decodificador** aquí descrito está en cambio escrito desde cero, y usa
ese código y ese CRC sin modificaciones, por decisión: cambiarlos rompería la
compatibilidad bit a bit con las demás estaciones.

El decodificador trabaja en dos etapas. La primera es un *min-sum* normalizado y
estratificado, que cierra la mayoría de las palabras con buena señal. La segunda
es un OSD, *ordered statistics decoding*, que vuelve a examinar sólo las
palabras que la primera no cerró: permuta las columnas poniendo delante los bits
menos fiables, resuelve, y prueba cierto número de variantes en torno a la
solución. La magnitud que decide cuánto cuesta y cuánto rinde la segunda etapa
es **el número de candidatos que prueba**. Y ahí está el resultado.

---

## 02 · Velocidad: min-sum vectorizado y decodificación por bloques

La primera etapa se reescribió con intrínsecas AVX2, dieciséis palabras por
registro de enteros de 16 bits, en coma fija Q=1/8 con saturación. Las palabras
ya convergidas salen del bucle. La segunda etapa usa eliminación gaussiana sin
saltos condicionales sobre filas de 256 bits, poda por cota inferior,
ordenación radix y síndrome CRC incremental.

La ganancia que hace posible todo lo demás es la de la primera etapa: de **139,8
a 4,7 microsegundos** por palabra, veintinueve veces y media. No es un fin en sí
mismo: sirve para hacer practicable un orden OSD más alto, y de ahí viene la
sensibilidad.

LDPC(174,91) sobre AWGN/BPSK · 20 000 palabras por punto · un hilo ·
Ryzen Zen 3, gcc 15.2, `-O3 -march=native`:

| Eb/N0 | FER rápido | µs | FER conservador | µs | FER sensible | µs |
|---:|---:|---:|---:|---:|---:|---:|
| 0,5 dB | 0,851 | 6,3 | 0,401 | 19,0 | **0,265** | 39,6 |
| 1,0 dB | 0,674 | 5,5 | 0,209 | 15,3 | **0,114** | 31,8 |
| 1,5 dB | 0,443 | 5,5 | 0,082 | 12,2 | **0,036** | 21,8 |
| 2,0 dB | 0,223 | 5,3 | 0,024 | 8,9 | **0,0082** | 14,3 |
| 2,5 dB | 0,083 | 4,6 | 0,0050 | 6,1 | **0,0018** | 7,5 |
| 3,0 dB | 0,021 | 2,9 | 0,00095 | 3,2 | **0,00010** | 4,1 |

Respecto a la configuración de partida la ganancia de sensibilidad es de
**+0,35 dB**, y de **+1,3 dB** respecto al min-sum solo, con igual tasa de falsas
decodificaciones, y con la cadena dieciséis veces más rápida a 2 dB.

### Esperado frente a medido — la decodificación por bloques

| Previsión | Medición |
|---|---|
| Seis veces más rápido: el min-sum pasa de una palabra por carril a dieciséis. | **1,8 veces.** La cuenta tenía en cuenta sólo el min-sum; el OSD sigue siendo por palabra y pasa a ser la parte dominante. |

La ganancia existe y es real, pero es un tercio de la anunciada. La previsión
era errónea porque optimizaba mentalmente la parte que ya era rápida.

---

## 03 · FT8: el mismo decodificador, la misma ganancia

FT8 y FT2 comparten el código, así que comparten el decodificador. En Decodium 4
la ruta de FT8 usa `fastldpc` incluida la decodificación por bloques de las
pasadas, con un interruptor de entorno (`DECODIUM_FT8_FASTLDPC=0`) para volver
al decodificador original y un mecanismo de recuperación que, para un número
limitado de candidatos por ciclo, reintenta con el clásico.

Una diferencia respecto a FT2 merece subrayarse porque es sustancial: el filtro
de plausibilidad descrito más adelante se ejecuta en FT8 con **todos los tipos
de mensaje admitidos**. Los formatos de concurso que en FT2 no se ven, en FT8
existen de verdad, y filtrarlos dejaría ciego al decodificador precisamente en
los días de concurso.

Etapa FT8 de producción sobre una ranura generada con `ft8sim` a −18 dB,
profundidad 3, dos vueltas por configuración:

| Decodificador LDPC | Vuelta 1 | Vuelta 2 | Decodificaciones |
|---|---:|---:|---:|
| Original `ftx_decode174_91_c` | 71 648 ms | 71 373 ms | 3 |
| **`fastldpc`** | **9 316 ms** | **9 319 ms** | 3 |

**7,7 veces más rápido, con decodificaciones idénticas.** La repetibilidad está
dentro del 0,4%, y las mismas tres líneas con el indicativo salen de ambas
configuraciones: la ganancia es toda tiempo, sin sacrificar sensibilidad.

El número debe leerse por lo que es. `ft8_stage_compare` ejecuta la etapa de
producción comparando varias configuraciones sobre el mismo fichero, así que es
una carga sesgada hacia el decodificador: es la relación correcta para la parte
LDPC, no el tiempo de la aplicación completa. Aun así es la pieza que, en los
ciclos cargados, decide si el ciclo cierra a tiempo.

### El umbral en dB, que es la métrica de verdad

El número que de verdad cuenta para FT8 no es el tiempo sino el **umbral al
50%**: la SNR a la que se decodifica la mitad de las señales, con señal
implantada a SNR conocida en el ancho de banda de referencia de 2500 Hz. El
recuento total de decodificaciones no es una métrica: está inflado por las
señales fáciles.

Medido con `decode_bench/`, que genera las señales con `ft8sim` de WSJT-X y por
tanto tiene verdad de referencia. Siete puntos de −19 a −25 dB, 25
realizaciones de ruido cada uno, perfil deep, mensaje `K1ABC W9XYZ EN37` a
1500 Hz:

| SNR | con `fastldpc` | decodificador original | `jt9` deep |
|---:|---:|---:|---:|
| −19 dB | 25/25 | 25/25 | 25/25 |
| −20 dB | 24/25 | 23/25 | 23/25 |
| −21 dB | **11/25** | 7/25 | 9/25 |
| −22 dB | **6/25** | 3/25 | 7/25 |
| −23 dB | 0/25 | 0/25 | 0/25 |

| | umbral al 50% |
|---|---:|
| Decodium con **`fastldpc`** | **−20,88 dB** |
| Decodium con el decodificador original | −20,66 dB |
| `jt9` de WSJT-X, perfil deep | −20,75 dB |

**El factor 7,7 de velocidad no cuesta sensibilidad.** `fastldpc` resulta 0,22 dB
más sensible que el decodificador original y 0,13 dB más que `jt9`.

Sobre la fuerza estadística hay que decir la verdad: los dos puntos informativos
son −21 dB (11/25 frente a 7/25) y −22 dB (6/25 frente a 3/25), cada uno a unos
1,2 sigma, que combinados dan alrededor de 1,7. Sugerente, no concluyente. **Lo
que puede afirmarse sin reservas es que fastldpc no cuesta sensibilidad**; para
establecer los +0,2 dB harían falta un centenar de realizaciones por punto en
lugar de veinticinco.

### El control: no es el plazo

El banco impone un plazo por decodificación, y el decodificador original es 7,7
veces más lento: la sospecha obvia es que la diferencia no sea calidad sino
tiempo agotado. Es una hipótesis comprobable, y hay que comprobarla, porque
cambia por completo qué se está midiendo.

Dos puntos (−21 y −22 dB), 40 realizaciones, perfil deep:

| | umbral | −21 dB | −22 dB | tiempo total |
|---|---:|---:|---:|---:|
| `fastldpc`, plazo 8 s | **−21,29 dB** | 24/40 | 10/40 | 547 s |
| original, plazo 8 s | −21,00 dB | 20/40 | 5/40 | 647 s |
| original, plazo **40 s** | −21,05 dB | 21/40 | 1/40 | **3208 s** |

**Dando al decodificador original cinco veces más tiempo no cambia nada**:
−21,05 frente a −21,00. El plazo no era la restricción, y la hipótesis era
errónea. La diferencia es calidad del decodificador, no tiempo agotado —
coherente con la cadena descrita arriba: la velocidad no regala decibelios por
sí sola, permite *permitirse* un orden de búsqueda más alto, y son ésos los que
los dan.

Dos observaciones sobre la solidez. Esta tanda da una diferencia de 0,29 dB, la
anterior 0,22: dos muestras independientes coincidentes en dirección y magnitud,
que juntas llevan la señal a unos 2,4 sigma. Y el umbral absoluto oscila 0,4 dB
entre dos tandas de la misma configuración (−20,88 y −21,29), lo que recuerda lo
poco que pesan veinticinco o cuarenta realizaciones por punto: lo que aguanta
son las comparaciones apareadas, no los valores absolutos.

Hay por último un detalle que conviene señalar sin forzarlo: a −22 dB el
decodificador original con más tiempo lo hace **peor**, 1/40 frente a 5/40. Los
números son pequeños y la diferencia está a 1,7 sigma, así que poco puede
concluirse — pero la dirección es exactamente la de la tesis de este informe:
más tiempo significa más candidatos probados, y más candidatos significa más
falsos positivos del CRC que ahogan al correcto.

También vale la lectura opuesta, y es la más útil: Decodium está **a la par con
`jt9` en perfil deep**. En FT8, los decibelios ya no están en el
decodificador.

---

## 04 · Cuántos decibelios quedan en el decodificador

Optimizar un decodificador supone implícitamente que buscando mejor se
decodifica más. No está dicho, y es algo que se puede medir en vez de suponer.

Un fallo tiene dos causas opuestas. O la palabra verdadera era *más verosímil*
que la elegida y el decodificador no la encontró — entonces buscar más compensa.
O la palabra verdadera era *menos verosímil* que otra palabra de código válida:
ahí se equivocó la máxima verosimilitud misma, ningún decodificador puede
hacerlo mejor, y los decibelios hay que buscarlos en el demodulador o en el
sincronismo.

En canal AWGN la verosimilitud de una palabra es la suma de los valores
absolutos de los LLR en los bits donde contradice la decisión dura. Comparando
la de la palabra verdadera con la de la elegida, a Eb/N0 = 1 dB sobre 5000
palabras, con la configuración casi óptima (orden 3, span 91/48, gate apagado):

| Causa | Casos | Cuota |
|---|---:|---:|
| Límite del **código** — la palabra verdadera era menos verosímil | 5 | 1,1% |
| Límite de **búsqueda** — palabra errónea aceptada | 370 | 79,4% |
| Límite de **búsqueda** — ninguna palabra encontrada | 91 | 19,5% |

El **98,9% de los fallos es un límite de búsqueda**. Sobre el papel había mucho
que tomar: ampliando el OSD se pasaba del 84,96% al 90,68% de decodificaciones
correctas, a 1,7 veces el coste.

La misma medición, aplicada a un código LDPC cuántico en régimen de *code
capacity*, da la respuesta opuesta: **cero fallos recuperables**, es decir un
decodificador ya en el techo de la decodificación de peso mínimo. Es un criterio
sencillo para decidir si conviene trabajar en el decodificador o parar, y merece
la pena ejecutarlo *antes* de cualquier optimización.

---

## 05 · El resultado: lo que limita es la prueba de aceptación

Esos 5,7 puntos porcentuales de decodificaciones adicionales no son cobrables
así, porque llegan junto con un montón de falsas decodificaciones. La pregunta
correcta no es cuántos candidatos se prueban, sino cuántos se aceptan por error.

La única prueba que decide si un candidato es válido es el CRC de 14 bits, que
admite un candidato erróneo cada 2¹⁴ = 16 384. La búsqueda estrecha prueba unos
600 candidatos por palabra, la ancha unos 21 400. De ahí sale un número esperado
de falsos positivos de CRC por palabra de **0,04 frente a 1,3**.

> Ampliar la búsqueda compra candidatos correctos y falsos en la misma
> proporción. El cuello de botella no es la búsqueda: es la prueba de aceptación.

La verificación consiste en poner ambas anchuras sobre la misma curva, barriendo
el umbral del gate antifantasma, con 20 000 palabras con señal y 100 000
candidatos de puro ruido. La comparación debe hacerse **a igualdad de
fantasmas**, no a igualdad de umbral: el umbral no es la magnitud que le importa
a nadie.

Eb/N0 = 1 dB:

| Configuración | Umbral | Decodificaciones | Fantasmas |
|---|---:|---:|---:|
| **Estrecha, sin filtro** *(en servicio)* | 0,065 | **16 168** | **10** |
| Estrecha, sin filtro | 0,070 | 16 718 | 43 |
| Estrecha, sin filtro | 0,075 | 16 924 | 141 |
| Ancha, sin filtro | 0,065 | 16 810 | 20 |
| Ancha, sin filtro | 0,070 | 17 564 | 81 |
| Ancha, sin filtro | 0,075 | 17 912 | 311 |

A igualdad de fantasmas ambas anchuras se equivalen, y por debajo del empate la
estrecha es mejor. El punto de trabajo en servicio está justo por debajo.
**Ampliar la búsqueda, por sí sola, no compensa.**

---

## 06 · Dos bits de estructura del mensaje

Si la restricción es la prueba de aceptación, se refuerza la prueba. Y la
información para hacerlo ya está ahí: los 77 bits de la carga útil no son un
número cualquiera, son un mensaje con una estructura. Una carga útil sorteada al
azar casi nunca describe indicativos posibles.

La comprobación verifica solamente restricciones estructurales seguras:

- que el tipo de mensaje `i3` esté entre los definidos y admitidos, y para
  `i3=0` también el subtipo `n3`;
- que los campos de longitud limitada estén dentro de su rango — el texto libre
  es 42¹³ dentro de 71 bits, el indicativo no estándar 38¹¹ dentro de 58, el
  intercambio ARRL 1..8000 o un multiplicador válido;
- que los indicativos tengan estructura posible: sufijo alineado a la izquierda,
  al menos una letra de sufijo, prefijo que no sea dos cifras ni una cifra sola;
- que un token (CQ, DE, QRZ) no aparezca en segunda posición.

Ninguna comprobación geográfica ni estadística: ésas rechazarían contactos
reales.

Está **dentro** del bucle de aceptación del OSD, no después. La distinción es
sustancial: un candidato falso que pasa el CRC pero no es un mensaje no detiene
la enumeración, que todavía puede encontrar el correcto. Aplicado después, se
limitaría a descartar la palabra dejando el hueco. Cuesta casi nada porque sólo
lo ven los candidatos que ya han pasado el CRC, es decir uno de cada 16 384.

Fuerza del filtro, sobre 2 millones de cargas útiles sorteadas al azar:

| Ajuste | Aceptados | Bits de filtro | Factor |
|---|---:|---:|---:|
| Todos los tipos definidos *(usado en FT8)* | 0,573 | **0,80** | 1,74× |
| Sólo los tipos usados *(usado en FT2)* | 0,268 | **1,90** | 3,73× |

### La ganancia, y su seguridad

A igualdad de búsqueda y de umbral, el filtro **reduce a la mitad los
indicativos fantasma dejando las decodificaciones idénticas**. Es la parte
adoptable sin contrapartidas: no cambia ni la búsqueda ni el umbral, y no puede
quitar decodificaciones.

| Filtro | Decodificaciones | Fantasmas / 100 000 |
|---|---:|---:|
| Ninguno | 16 168 | 10 |
| Todos los tipos definidos | 16 168 | **7** |
| **Sólo los tipos usados** | **16 170** | **4** |

La seguridad está verificada en dos frentes. Sobre 20 000 mensajes realistas el
filtro no descarta ninguno. Y sobre 404 indicativos tomados de registros ADIF
reales, tras la corrección de más abajo, ni uno.

### Esperado frente a medido — la regla del prefijo

| Regla escrita | Validada contra los registros |
|---|---|
| Antes de la cifra del indicativo va una letra. Parece obvia. | **Descarta 12 indicativos reales de 404**: S53MJ, S50XX, A61OK, S51RU, S56EPX, S51DM, Z31B, N25BRX, G56KAY, A65DF, Z62NS, E75AA. |

Son prefijos letra+cifra: **S5** Eslovenia, **A6** Emiratos Árabes Unidos,
**Z3** Macedonia del Norte, **E7** Bosnia, **Z6** Kosovo. Con esa regla esos
países no se habrían vuelto a decodificar nunca. La restricción verdadera es más
débil: un prefijo puede ser letra, letra+letra, letra+cifra o cifra+letra, nunca
dos cifras ni una cifra sola.

> Un filtro de plausibilidad se valida contra datos reales, no contra el propio
> razonamiento. El banco que lo detectó es `lab/tools/valida_nominativi.py`, y
> hay que volver a ejecutarlo cada vez que se toca la regla.

---

## 07 · Cuando el laboratorio se equivoca y la banda corrige

Con dos bits más de filtro, la búsqueda ancha volvía a ser conveniente sobre el
papel. La medición en laboratorio daba, a igualdad de fantasmas, **+4,0% de
decodificaciones y −40% de fantasmas a la vez**: una mejora neta en ambos ejes,
sin contrapartidas. Se llevó a producción.

### Esperado frente a medido — la búsqueda ancha en el aire

| Laboratorio | En el aire |
|---|---|
| Sobre ruido gaussiano sintético: más decodificaciones *y* menos fantasmas. Probada dos veces. | **Retirada dos veces.** Seis minutos con cero fantasmas parecían absolverla, pero en una banda sin tráfico FT2 seis minutos no demuestran nada: con más tiempo los fantasmas volvieron en abundancia. |

El ruido real no es gaussiano blanco. Portadoras, otros modos y QRM producen
LLR correlados a los que el OSD se agarra, y una búsqueda ancha encuentra
estructura donde el modelo sintético no la tenía. **Un banco sobre ruido
sintético puede sobreestimar, y en este caso lo hizo.**

La configuración en producción se queda con la búsqueda estrecha y el filtro
activo: la ganancia que sobrevive es la reducción a la mitad de los fantasmas.

---

## 08 · La digresión cuántica, y por qué se detiene aquí

El mismo núcleo — min-sum vectorizado más OSD sobre la base más fiable — se
llevó a decodificar síndromes de códigos LDPC cuánticos: *bivariate bicycle* de
Bravyi et al., incluido el [[144,12,12]], tanto en régimen de code capacity como
con ruido de circuito obtenido con `stim`.

Tres diferencias respecto al caso clásico. El síndrome no es cero, y en el
min-sum eso se convierte en una línea: el acumulador de signos parte de
*s<sub>m</sub>* en vez de cero. No hay ningún CRC, y no hace falta, porque todo
candidato satisface el síndrome por construcción. El éxito no es recuperar el
error: un código cuántico es degenerado, y la decodificación es correcta siempre
que el residuo no contenga operadores lógicos.

El trabajo produjo resultados verificables — entre ellos la demostración, por
enumeración exhaustiva, de que ningún calendario uniforme de seis capas puede
extraer el síndrome de los códigos BB de forma determinista, mientras que con
siete existen 236 válidos, y el adoptado conserva la distancia del código.

### Esperado frente a medido — la comparación con la literatura

| Convicción inicial | Bibliografía |
|---|---|
| Terreno poco transitado, y de 100 a 600 veces más rápido que la referencia. | Campo concurrido y en movimiento. **La biblioteca usada como referencia ya contiene el decodificador rápido**, *Localized Statistics Decoding*, creado precisamente para este problema: se había medido contra el lento. |

La comparación se rehízo entonces sobre **`sinter`**, el banco estándar del
campo: por lotes, multiproceso, la misma infraestructura para todos los
decodificadores, y reproducible por cualquiera sin tener nuestro código. Surface
code d=5, 50 000 disparos por punto, seis procesos:

| p | fastldpc | BP+LSD | BP+OSD-7 | pymatching |
|---:|---:|---:|---:|---:|
| 0,001 | 3 err · 190 µs | 4 · 1107 µs | 1 · 1039 µs | 7 · 0,9 µs |
| 0,002 | **17** · 119 µs | 30 · 2001 µs | 29 · 2570 µs | 59 · 1,5 µs |
| 0,003 | **86** · 141 µs | 124 · 3268 µs | 97 · 4485 µs | 153 · 2,1 µs |

**De 8 a 32 veces más rápido que BP+LSD y BP+OSD, con menos errores que ambos**,
y la ventaja de velocidad está infravalorada: nuestro tiempo incluye toda la
vuelta — análisis del modelo en Python, escritura de ficheros, arranque del
proceso — mientras que los demás se ejecutan en el mismo proceso.

Sobre la exactitud hay que declarar la fuerza estadística: a p=0,002 el 17 frente
a 29 está a 1,8 sigma; a p=0,003 el 86 frente a 97 a 0,8. Tomados de uno en uno
no son concluyentes; estar por debajo en los tres puntos lo es más. A la par o
ligeramente mejor, con la velocidad como ventaja sólida.

> **El banco estándar detectó de inmediato un fallo que tres días de mediciones
> internas no habían visto.** La primera medición con sinter daba 768 errores
> donde el banco interno daba 98: factor ocho. sinter entrega a los
> decodificadores el modelo **descompuesto**, donde una instrucción se parte con
> `^`, y un observable presente en dos componentes **se anula** en GF(2).
> Recogiendo los objetivos en una lista en vez de por paridad, 86 instrucciones
> de 1953 quedaban marcadas como si volcaran el observable. Un error silencioso:
> pesos, probabilidades previas y masa total no cambiaban. Sólo salió a la luz
> porque la integración nueva no reproducía un número ya conocido — y ésa es la
> comprobación que debía haber sido la primera.

La ventana deslizante implementada para hacer lineal el coste con el número de
rondas es técnica estándar, y la profundidad 7 del calendario ya estaba en el
trabajo de Bravyi et al.: se ha redescubierto, no descubierto.

---

## 09 · Qué es nuevo y qué no

**No es nuevo el concepto.** Aprovechar la redundancia residual de la fuente
dentro del decodificador de canal es *source-controlled channel decoding*, una
línea que se remonta a Hagenauer en los años noventa. Y que el presupuesto de
falsos positivos del CRC limite el tamaño de la lista es teoría de códigos
estándar, bien documentada en la literatura sobre CRC-aided list decoding de
códigos polares y convolucionales.

**Es nueva la aplicación, y sobre todo la medición.** En la comunidad de FT8 y
WSJT-X no consta que nadie haya construido la curva
decodificaciones-frente-a-fantasmas y mostrado que lo que limita es la prueba de
aceptación y no la búsqueda; ni el resultado operativo que se deriva, es decir
la reducción a la mitad de los fantasmas sin cambiar las decodificaciones.
También es nuevo, por lo que consta, el uso del diagnóstico «límite de búsqueda
o límite del código» como criterio para decidir *dónde* invertir esfuerzo antes
de invertirlo.

---

## 10 · Limitaciones declaradas

- Las mediciones de FT2 usan LLR sintéticos AWGN, no la salida del demodulador
  4-GFSK real. El modelo de ruido **ya ha sobreestimado una vez**, como se
  describe en la sección 07.
- Los tiempos son sobre una sola máquina, un solo hilo, un solo compilador.
- Restringir a los tipos de mensaje realmente usados es una **política**, no una
  prueba de formato: un mensaje de un tipo excluido no se decodificaría nunca.
  En FT8 no se aplica, precisamente por eso.
- La comparación cuántica se ejecuta ahora sobre `sinter`, pero la ventaja de
  exactitud está a 1-2 sigma: hacen falta más disparos para consolidarla. Sobre
  códigos BB la comparación aún no se ha rehecho en el banco estándar.
- El umbral de FT8 en dB ya está medido (sección 03), pero sobre 25
  realizaciones por punto: la ventaja de 0,2 dB está a 1,7 sigma y no queda
  establecida. Hace falta una muestra cuatro veces mayor.

---

## 11 · Reproducir las mediciones

Cada tabla de este informe procede de un comando. Los bancos son header-only y
se compilan con `g++ -O3 -march=native -std=c++17`.

```
lab/cpp/ml_gap.cpp        limite de busqueda o limite del codigo
lab/cpp/pareto.cpp        curva decodificaciones frente a fantasmas
lab/cpp/noise_test.cpp    aceptaciones sobre puro ruido, con los tiempos
lab/cpp/plausible.hpp     la comprobacion de estructura del mensaje
lab/tools/gen_test.py     palabras de prueba; --reali para mensajes reales
lab/tools/valida_nominativi.py
                          verifica el filtro contra indicativos de logs ADIF
decode_bench/             umbral FT8 en dB con verdad de referencia
```

Una advertencia que costó tiempo: los datos de prueba generados **sin**
`--reali` contienen cargas útiles aleatorias, no mensajes. Medir un filtro de
plausibilidad sobre ésas lo hace parecer destructivo, porque descarta también
las palabras «correctas», que no son mensajes.

---

## 12 · Bibliografía

- R. G. Gallager, **«Low-Density Parity-Check Codes»**, MIT, 1962.
  El origen de la clase de códigos sobre la que todo esto se apoya.
  *IRE Trans. Inf. Theory, IT-8, pp. 21–28.*
- S. Franke K9AN, B. Somerville AE6NZ, J. Taylor K1JT, **«The FT4 and FT8
  Communication Protocols»**, QEX. La especificación del código LDPC(174,91) y
  del CRC de 14 bits usados sin modificaciones en este trabajo.
  <https://wsjt.sourceforge.io/FT4_FT8_QEX.pdf>
- **Joint Source–Channel Decoding**, Wiley. La línea del *source-controlled
  channel decoding*: aprovechar la redundancia residual de la fuente dentro del
  decodificador de canal.
  <https://onlinelibrary.wiley.com/doi/10.1002/9781118693803.ch10>
- **Design, Performance, and Complexity of CRC-Aided List Decoding of
  Convolutional and Polar Codes for Short Messages**. El compromiso entre tamaño
  de lista y probabilidad de error no detectado.
  <https://arxiv.org/pdf/2302.07513>
- **Pre-configured Error Pattern Ordered Statistics Decoding for CRC-Polar
  Codes**. <https://arxiv.org/pdf/2309.11836>
- **Linear-Equation Ordered-Statistics Decoding**. Variantes de baja complejidad
  del OSD. <https://arxiv.org/pdf/2110.11574>
- **Localized statistics decoding for quantum low-density parity-check codes**,
  Nature Communications, 2025. Ataca el coste de la eliminación gaussiana del
  OSD; distribuido en la biblioteca `ldpc`.
  <https://arxiv.org/pdf/2406.18655>
- **Ambiguity Clustering: an accurate and efficient decoder for qLDPC codes**.
  <https://arxiv.org/pdf/2406.14527>
- **Fully Parallelized BP Decoding for Quantum LDPC Codes Can Outperform
  BP-OSD**. <https://arxiv.org/abs/2507.00254>
- **An almost-linear time decoding algorithm for quantum LDPC codes under
  circuit-level noise**. La decodificación por ventana deslizante como técnica
  consolidada. <https://arxiv.org/pdf/2409.01440>
- **BP+OSD**, la biblioteca de referencia usada en las comparaciones.
  <https://github.com/quantumgizmos/bp_osd>

---

*Informe técnico sobre Decodium 4.0 Core Shannon · `fastldpc` · GPL-3.0.*

**Atribución.** `fastldpc` es un decodificador escrito desde cero para Decodium
4.0 Core Shannon. Implementa algoritmos conocidos — códigos LDPC (Gallager,
1962), min-sum normalizado, ordered statistics decoding — con vectorización AVX2
y optimizaciones originales. La búsqueda por pares está modelada sobre los pasos
`npre1`/`npre2` de `osd174_91` de WSJT-X. Opera sobre el código LDPC(174,91) y
el CRC de 14 bits (`0x2757`) del protocolo FT8, diseñados por Steve Franke K9AN
y Joe Taylor K1JT, usados sin modificaciones para garantizar compatibilidad bit
a bit. El decodificador original sigue disponible y seleccionable.
