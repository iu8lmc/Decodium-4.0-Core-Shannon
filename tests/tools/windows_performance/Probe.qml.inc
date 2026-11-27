    Timer {
        id: perfStart; interval: 12000; running: true; repeat: false
        property int step: 0
        property var fixtures: __FIXTURES__
        onTriggered: {
            if (step >= fixtures.length) {
                console.log('[PERFTEST] '+JSON.stringify({event:'done',epoch:Date.now()}));
                Qt.quit(); return;
            }
            var item=fixtures[step];
            bridge.mode=item.mode==='FT8' ? 'FT4' : 'FT8';
            bridge.mode=item.mode;
            bridge.filtersBypassed=true; bridge.autoSeq=false;
            bridge.ftThreadsAuto=false; bridge.ftThreads=__THREADS__;
            bridge.lowCpuModeEnabled=__LOWCPU__;
            bridge.ndepth=3; bridge.deepSearchEnabled=false; bridge.coherentAvgEnabled=false;
            perfWait.started=Date.now(); perfWait.lastChange=Date.now(); perfWait.signature='';
            perfWait.firstRowMs=-1; perfWait.sawBusy=false;
            console.log('[PERFTEST] '+JSON.stringify({event:'start',caseId:item.id,mode:bridge.mode,
                epoch:Date.now(),threads:bridge.ftThreads,lowCpu:bridge.lowCpuModeEnabled,
                depth:bridge.ndepth,cleared:bridge.decodeList.length===0}));
            perfWait.start();
            bridge.openWavForDecode(item.path);
        }
    }
    Timer {
        id: perfWait; interval:100; repeat:true
        property double started:0
        property double lastChange:0
        property string signature:''
        property int firstRowMs:-1
        property bool sawBusy:false
        onTriggered: {
            var now=Date.now(), elapsed=now-started;
            sawBusy=sawBusy || bridge.decoding;
            var rows=[], source=bridge.decodeList;
            for(var i=0;i<source.length;i++) {
                var r=source[i];
                rows.push({message:String(r.message).trim(),mode:String(r.mode),snr:String(r.db),dt:r.dt,freq:r.freq});
            }
            var sig=JSON.stringify(rows);
            if(sig!==signature) { signature=sig; lastChange=now; }
            if(rows.length && firstRowMs<0) firstRowMs=elapsed;
            var timeout=elapsed>=60000;
            if(timeout || (elapsed>=15000 && !bridge.decoding && now-lastChange>=3000 && (sawBusy || rows.length))) {
                stop();
                var item=perfStart.fixtures[perfStart.step];
                console.log('[PERFTEST] '+JSON.stringify({event:'result',caseId:item.id,epoch:now,
                    elapsedMs:elapsed,firstRowMs:firstRowMs,rows:rows,timeout:timeout,
                    tx:bridge.transmitting,stillDecoding:bridge.decoding}));
                if(timeout || bridge.transmitting) { Qt.quit(); return; }
                perfStart.step++;perfStart.interval=5000;perfStart.start();
            }
        }
    }
    Connections {
        target: bridge
        function onDecodingChanged() { if(perfWait.running && bridge.decoding) perfWait.sawBusy=true; }
    }
