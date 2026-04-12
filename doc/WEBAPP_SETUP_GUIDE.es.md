# Guia Web App (Principiantes) - Espanol

Esta guia explica, paso a paso, como activar y usar el dashboard web remoto de Decodium desde telefono/tablet/PC en la misma red local.

## 1) Requisitos

- Decodium `v1.4.3` ejecutandose en el ordenador de estacion.
- Ordenador de estacion y telefono/tablet conectados a la misma LAN/Wi-Fi.
- App principal Decodium funcionando primero (CAT/audio/decode).

## 2) Activar Dashboard Web Remoto en Decodium

1. Abre Decodium.
2. Ve a `Settings` -> `General`.
3. Busca la seccion: `ATTENZIONE: SEZIONE SPERIMENTALE` / `Remote Web Dashboard (LAN)`.
4. Activa: `Enable remote web dashboard`.
5. Configura:
   - `HTTP port`: por defecto `19091` (recomendado).
   - `WS bind address`: usa `0.0.0.0` para acceso LAN.
   - `Username`: deja `admin` (o define el tuyo).
   - `Access token (password)`: define una password fuerte.
6. Pulsa `OK`.
7. Reinicia Decodium (recomendado cuando cambies estos campos).

## 3) Obtener la IP del ordenador de estacion

En terminal del ordenador de estacion:

- macOS/Linux:
```bash
ipconfig getifaddr en0
```
(si no devuelve nada, prueba `en1`)

Ejemplo: `192.168.1.4`

## 4) Abrir dashboard desde otro dispositivo

En navegador del telefono/tablet abre:

```text
http://<ip-estacion>:19091
```

Ejemplo:

```text
http://192.168.1.4:19091
```

Inicia sesion con:

- Usuario: el configurado en Decodium (default `admin`)
- Password: tu `Access token`

## 5) Anadir a pantalla de inicio (PWA)

- iPhone/iPad (Safari): Compartir -> `Anadir a pantalla de inicio`
- Android (Chrome): menu -> `Instalar app` o `Anadir a pantalla de inicio`

Despues podras abrirla como icono de app normal.

## 6) Que puedes hacer desde la web app

- Ver estado en vivo: modo, banda, dial, frecuencias RX/TX, estado TX/monitor.
- Cambiar modo y banda.
- Ajustar frecuencia RX.
- Activar/desactivar TX.
- Activar/desactivar Auto-CQ.
- Lanzar accion remota slot-aware sobre un caller.
- Ver tabla de actividad RX/TX (botones pausa/limpiar).
- Usar waterfall remoto opcional (experimental).

## 7) Recomendaciones de seguridad (importante)

- Mantener el servicio solo en LAN.
- Usar token/password robusto.
- No exponer el puerto del dashboard a internet/router si no controlas bien la seguridad.
- Si puedes, limita firewall a dispositivos locales de confianza.

## 8) Solucion rapida de problemas

### El dashboard no abre

- Comprueba que Decodium esta ejecutandose.
- Comprueba que `Enable remote web dashboard` esta activo.
- Comprueba IP/puerto correctos.
- Comprueba `WS bind address = 0.0.0.0` para uso LAN.
- Reinicia Decodium tras cambios de settings remotos.

### Fallo de login

- Revisa usuario/password en settings de Decodium.
- Evita espacios al final en usuario/token.
- Guarda y reinicia Decodium.

### Abre en el PC pero no en el movil

- Puede haber VLAN/red guest distinta.
- Firewall local puede bloquear puerto `19091`.
- Verifica que la IP de la estacion no haya cambiado (DHCP).

### Web app conectada pero comandos no aplican

- Verifica monitor activo en Decodium.
- Verifica CAT operativo en app principal.
- Si no estas en FT2, los controles FT2-only pueden estar ocultos/limitados.

## 9) Endpoints API (opcional avanzado)

- Health:
```text
GET /api/v1/health
```
- Comandos:
```text
POST /api/v1/commands
```

Los endpoints usan la misma autenticacion que el dashboard web.
