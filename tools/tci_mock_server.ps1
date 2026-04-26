param(
    [int]$Port = 50001,
    [string]$LogPath = "$env:TEMP\decodium_tci_mock.log"
)

$ErrorActionPreference = 'Stop'

function Write-MockLog {
    param([string]$Message)
    $line = "[{0}] {1}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'), $Message
    try {
        Add-Content -LiteralPath $LogPath -Value $line -Encoding UTF8 -ErrorAction Stop
    } catch {
        # Keep the websocket alive even if another process is tailing the log.
    }
    Write-Host $line
}

function Read-ExactBytes {
    param(
        [System.Net.Sockets.NetworkStream]$Stream,
        [int]$Count
    )
    $buffer = New-Object byte[] $Count
    $offset = 0
    while ($offset -lt $Count) {
        $read = $Stream.Read($buffer, $offset, $Count - $offset)
        if ($read -le 0) {
            throw 'socket closed'
        }
        $offset += $read
    }
    return $buffer
}

function Send-Bytes {
    param(
        [System.Net.Sockets.NetworkStream]$Stream,
        [byte[]]$Bytes
    )
    $Stream.Write($Bytes, 0, $Bytes.Length)
    $Stream.Flush()
}

function Send-WebSocketText {
    param(
        [System.Net.Sockets.NetworkStream]$Stream,
        [string]$Text
    )
    $payload = [System.Text.Encoding]::UTF8.GetBytes($Text)
    $frame = New-Object System.Collections.Generic.List[byte]
    $frame.Add(0x81)
    if ($payload.Length -le 125) {
        $frame.Add([byte]$payload.Length)
    } elseif ($payload.Length -le 65535) {
        $frame.Add(126)
        $frame.Add([byte](($payload.Length -shr 8) -band 0xff))
        $frame.Add([byte]($payload.Length -band 0xff))
    } else {
        throw 'payload too large for this mock'
    }
    $frame.AddRange($payload)
    Send-Bytes -Stream $Stream -Bytes $frame.ToArray()
    Write-MockLog "TX $Text"
}

function Send-WebSocketBinary {
    param(
        [System.Net.Sockets.NetworkStream]$Stream,
        [byte[]]$Payload
    )
    $frame = New-Object System.Collections.Generic.List[byte]
    $frame.Add(0x82)
    if ($Payload.Length -le 125) {
        $frame.Add([byte]$Payload.Length)
    } elseif ($Payload.Length -le 65535) {
        $frame.Add(126)
        $frame.Add([byte](($Payload.Length -shr 8) -band 0xff))
        $frame.Add([byte]($Payload.Length -band 0xff))
    } else {
        $frame.Add(127)
        for ($shift = 56; $shift -ge 0; $shift -= 8) {
            $frame.Add([byte](($Payload.Length -shr $shift) -band 0xff))
        }
    }
    $frame.AddRange($Payload)
    Send-Bytes -Stream $Stream -Bytes $frame.ToArray()
}

function Set-UInt32LE {
    param(
        [byte[]]$Buffer,
        [int]$Offset,
        [uint32]$Value
    )
    [BitConverter]::GetBytes($Value).CopyTo($Buffer, $Offset)
}

function Send-TciRxAudioBurst {
    param(
        [System.Net.Sockets.NetworkStream]$Stream,
        [uint32]$Receiver = 0
    )
    $sampleRate = [uint32]48000
    $formatFloat32 = [uint32]3
    $rxAudioStream = [uint32]1
    $channels = [uint32]2
    $sampleCount = [uint32]4096
    $headerBytes = 64
    $twoPi = 2.0 * [Math]::PI
    for ($chunk = 0; $chunk -lt 8; ++$chunk) {
        $payload = New-Object byte[] ($headerBytes + [int]$sampleCount * 4)
        Set-UInt32LE $payload 0 $Receiver
        Set-UInt32LE $payload 4 $sampleRate
        Set-UInt32LE $payload 8 $formatFloat32
        Set-UInt32LE $payload 12 0
        Set-UInt32LE $payload 16 0
        Set-UInt32LE $payload 20 $sampleCount
        Set-UInt32LE $payload 24 $rxAudioStream
        Set-UInt32LE $payload 28 $channels
        for ($i = 0; $i -lt [int]$sampleCount; ++$i) {
            $frame = [Math]::Floor($i / 2)
            $phase = $twoPi * (($chunk * ([int]$sampleCount / 2) + $frame) % 48) / 48.0
            [single]$value = [single]([Math]::Sin($phase) * 0.05)
            [BitConverter]::GetBytes($value).CopyTo($payload, $headerBytes + $i * 4)
        }
        Send-WebSocketBinary -Stream $Stream -Payload $payload
        Start-Sleep -Milliseconds 20
    }
    Write-MockLog "TX binary rx audio burst samples=$sampleCount chunks=8"
}

function Read-WebSocketText {
    param([System.Net.Sockets.NetworkStream]$Stream)
    $header = Read-ExactBytes -Stream $Stream -Count 2
    $opcode = $header[0] -band 0x0f
    $masked = ($header[1] -band 0x80) -ne 0
    [uint64]$length = $header[1] -band 0x7f
    if ($length -eq 126) {
        $ext = Read-ExactBytes -Stream $Stream -Count 2
        $length = ([uint64]$ext[0] -shl 8) -bor [uint64]$ext[1]
    } elseif ($length -eq 127) {
        $ext = Read-ExactBytes -Stream $Stream -Count 8
        $length = 0
        foreach ($b in $ext) {
            $length = ($length -shl 8) -bor [uint64]$b
        }
    }
    $mask = if ($masked) { Read-ExactBytes -Stream $Stream -Count 4 } else { @() }
    $payload = if ($length -gt 0) { Read-ExactBytes -Stream $Stream -Count ([int]$length) } else { @() }
    if ($masked) {
        for ($i = 0; $i -lt $payload.Length; ++$i) {
            $payload[$i] = $payload[$i] -bxor $mask[$i % 4]
        }
    }
    if ($opcode -eq 8) {
        throw 'websocket close frame'
    }
    if ($opcode -ne 1) {
        return ''
    }
    return [System.Text.Encoding]::UTF8.GetString($payload)
}

function Read-HttpHandshake {
    param([System.Net.Sockets.NetworkStream]$Stream)
    $bytes = New-Object System.Collections.Generic.List[byte]
    $one = New-Object byte[] 1
    while ($true) {
        $read = $Stream.Read($one, 0, 1)
        if ($read -le 0) {
            throw 'socket closed before handshake'
        }
        $bytes.Add($one[0])
        $count = $bytes.Count
        if ($count -ge 4 -and
            $bytes[$count - 4] -eq 13 -and
            $bytes[$count - 3] -eq 10 -and
            $bytes[$count - 2] -eq 13 -and
            $bytes[$count - 1] -eq 10) {
            break
        }
    }
    return [System.Text.Encoding]::ASCII.GetString($bytes.ToArray())
}

function Send-HttpHandshakeResponse {
    param(
        [System.Net.Sockets.NetworkStream]$Stream,
        [string]$Request
    )
    $match = [regex]::Match($Request, '(?im)^Sec-WebSocket-Key:\s*(.+?)\s*$')
    if (-not $match.Success) {
        throw 'missing Sec-WebSocket-Key'
    }
    $key = $match.Groups[1].Value.Trim()
    $guid = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11'
    $sha1 = [System.Security.Cryptography.SHA1]::Create()
    $hash = $sha1.ComputeHash([System.Text.Encoding]::ASCII.GetBytes($key + $guid))
    $accept = [Convert]::ToBase64String($hash)
    $response = "HTTP/1.1 101 Switching Protocols`r`n" +
        "Upgrade: websocket`r`n" +
        "Connection: Upgrade`r`n" +
        "Sec-WebSocket-Accept: $accept`r`n" +
        "`r`n"
    Send-Bytes -Stream $Stream -Bytes ([System.Text.Encoding]::ASCII.GetBytes($response))
}

function Send-InitialTciState {
    param([System.Net.Sockets.NetworkStream]$Stream)
    Send-WebSocketText -Stream $Stream -Text 'protocol:ExpertSDR3,2.0;device:SunSDR2DX;trx_count:2;channels_count:2;start;ready;'
    Send-WebSocketText -Stream $Stream -Text 'rx_enable:0,true;rx_enable:1,false;split_enable:0,false;split_enable:1,false;'
    Send-WebSocketText -Stream $Stream -Text 'vfo:0,0,14074000;vfo:0,1,14074000;vfo:1,0,14074000;vfo:1,1,14074000;'
    Send-WebSocketText -Stream $Stream -Text 'modulation:0,0,digu;modulation:1,0,digu;drive:0,50;rx_smeter:0,0,-54;'
}

function Handle-TciCommands {
    param(
        [System.Net.Sockets.NetworkStream]$Stream,
        [string]$Text
    )
    if ([string]::IsNullOrWhiteSpace($Text)) {
        return
    }
    Write-MockLog "RX $Text"
    foreach ($command in ($Text -split ';')) {
        $command = $command.Trim()
        if ($command.Length -eq 0) {
            continue
        }
        $name, $argsText = $command.Split(':', 2)
        $args = if ($argsText) { @($argsText.Split(',')) } else { @() }
        switch ($name.ToLowerInvariant()) {
            'start' {
                Send-WebSocketText -Stream $Stream -Text 'start;ready;'
            }
            'split_enable' {
                if ($args.Count -eq 1) {
                    Send-WebSocketText -Stream $Stream -Text "split_enable:0,$($args[0]);"
                } else {
                    Send-WebSocketText -Stream $Stream -Text "$command;"
                }
            }
            'vfo' {
                Send-WebSocketText -Stream $Stream -Text "$command;"
            }
            'modulation' {
                Send-WebSocketText -Stream $Stream -Text "$command;"
            }
            'trx' {
                Send-WebSocketText -Stream $Stream -Text "$command;"
                if ($args.Count -ge 2 -and $args[1].ToLowerInvariant() -eq 'true') {
                    Send-WebSocketText -Stream $Stream -Text 'tx_power:10.0;tx_swr:1.1;'
                } else {
                    Send-WebSocketText -Stream $Stream -Text 'tx_power:0.0;tx_swr:0.0;'
                }
            }
            'audio_start' {
                $rx = if ($args.Count -ge 1) { $args[0] } else { '0' }
                [uint32]$rxNumber = 0
                [void][uint32]::TryParse([string]$rx, [ref]$rxNumber)
                Send-WebSocketText -Stream $Stream -Text "audio_start:$rx;"
                Send-TciRxAudioBurst -Stream $Stream -Receiver $rxNumber
            }
            'audio_stop' {
                $rx = if ($args.Count -ge 1) { $args[0] } else { '0' }
                Send-WebSocketText -Stream $Stream -Text "audio_stop:$rx;"
            }
            'rx_smeter' {
                $rx = if ($args.Count -ge 1) { $args[0] } else { '0' }
                Send-WebSocketText -Stream $Stream -Text "rx_smeter:$rx,0,-54;"
            }
            'rx_sensors_enable' {
                Send-WebSocketText -Stream $Stream -Text 'rx_channel_sensors:0,0,-54.0;'
            }
            'tx_sensors_enable' {
                Send-WebSocketText -Stream $Stream -Text 'tx_sensors:0,0,0,0.0,0.0;'
            }
            default {
                Send-WebSocketText -Stream $Stream -Text 'ready;'
            }
        }
    }
}

if (Test-Path -LiteralPath $LogPath) {
    Remove-Item -LiteralPath $LogPath -Force
}

$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $Port)
$listener.Start()
Write-MockLog "TCI mock listening on ws://127.0.0.1:$Port"

try {
    while ($true) {
        $client = $listener.AcceptTcpClient()
        $remote = $client.Client.RemoteEndPoint.ToString()
        Write-MockLog "client connected from $remote"
        try {
            $stream = $client.GetStream()
            $request = Read-HttpHandshake -Stream $stream
            Write-MockLog "handshake request received"
            Send-HttpHandshakeResponse -Stream $stream -Request $request
            Write-MockLog "handshake accepted"
            Send-InitialTciState -Stream $stream
            while ($client.Connected) {
                $text = Read-WebSocketText -Stream $stream
                Handle-TciCommands -Stream $stream -Text $text
            }
        } catch {
            Write-MockLog "client ended: $($_.Exception.Message)"
        } finally {
            $client.Close()
        }
    }
} finally {
    $listener.Stop()
}
