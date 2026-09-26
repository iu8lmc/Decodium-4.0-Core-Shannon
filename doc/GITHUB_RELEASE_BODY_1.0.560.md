# Decodium 4 FT2 v1.0.560

This release continues directly from v1.0.559. It focuses on a clearer and
more accessible Settings interface, better behaviour of the automatic
waterfall threshold on GPU renderers, and precise wording for decoder options.

## English (British)

### Settings-only text scaling

The Setup window now includes three compact controls for its internal text
size: `A−`, `A+` and `Reset`. The scale starts at 100%, can be reduced no
further than 100%, and can be increased in 10% steps up to 130%.

The scale applies only to the Setup window and its embedded pages. It does not
change the typography or layout of the main operating dashboard. The responsive
breakpoints are calculated from the effective scaled content width, so the
Settings layout continues to select the appropriate compact or wide form while
the text size changes.

### Decometer resizing on Windows

The frameless Decometer window now uses Qt's native Windows resize operation
from its proportional corner grips, with the existing manual path retained on
macOS and other platforms. The hit area is larger for high-DPI displays, and
the final geometry is clamped back to the instrument's fixed 15:7 aspect ratio.

### Readable Settings controls in both themes

The top-level Settings window now receives the same Qt Material palette as the
main application. Controls that use the platform style no longer fall back to
a bright white button on the dark theme, and their labels and enabled or
disabled state remain readable. Light-theme Settings retain their light
contrast and accent colours.

### Automatic waterfall threshold on GPU renderers

The `Cut` control beside `Auto` now affects the GPU Direct waterfall path as
well as the CPU path. The existing 10% setting remains neutral, preserving the
previous calibration. Lower values expose weaker signals by lowering the
effective display threshold; higher values remove more of the noise floor.

The same mapping is used by the common GPU Direct implementation on Metal,
Direct3D 11, Direct3D 12 and Vulkan. OpenGL uses it when its GPU FFT/compute
path is enabled; its CPU fallback continues to use the original percentile
calculation. The stacked 3D history receives the same threshold through the
new waterfall rows, so the change becomes visible consistently in both 2D and
3D views.

The diagnostic log now records the selected Cut percentage and its dB bias,
making renderer-specific comparisons easier. The CPU diagnostic label also
reports the actual percentile instead of retaining the old fixed `p25` name.

### Clearer decoder terminology and filter labels

The decoder option previously labelled `Deep decode in TX` now explains its
actual scope: `Deep decode of last RX slot during TX (list only)`. This makes it
clear that the feature examines the most recent received slot during
transmission and only affects the displayed list.

The `W&P Filters Only` label is now rendered correctly instead of exposing the
HTML entity text `W&amp;P`. The revised decoder wording is carried through the
available translations, together with the corrected filter label.

### Validation

The release was checked with the focused update-support test suite (13 tests
passed), a complete `decodium_qml` build, QML and translation generation, and a
clean `git diff --check` result. Existing local changes are included in this
release commit.

## Italiano

### Ridimensionamento del testo limitato a Impostazioni

La finestra Setup ora contiene tre controlli compatti per la dimensione interna
del testo: `A−`, `A+` e `Reset`. Il valore iniziale è 100%, non può scendere
sotto il 100% e può aumentare a passi del 10% fino a un massimo del 130%.

La scala si applica soltanto alla finestra Setup e alle sue pagine interne. Non
modifica i caratteri né il layout della dashboard operativa principale. I
breakpoint responsive vengono calcolati sulla larghezza effettiva del
contenuto scalato, così il layout di Impostazioni continua a scegliere
correttamente la variante compatta o larga anche dopo la modifica del testo.

### Ridimensionamento di Decometer su Windows

La finestra Decometer senza cornice usa ora il ridimensionamento nativo di
Windows quando si trascinano gli angoli proporzionali; su macOS e sulle altre
piattaforme resta attivo il percorso manuale già esistente. La zona di aggancio
è stata ampliata per i monitor ad alta densità e la geometria finale viene
riportata al rapporto fisso 15:7 dello strumento.

### Controlli leggibili in Impostazioni con entrambi i temi

La finestra principale di Impostazioni riceve ora la stessa palette Material
Qt dell'applicazione. I controlli che utilizzano lo stile della piattaforma non
ricadono più su un pulsante bianco molto chiaro nel tema scuro, e il testo e lo
stato abilitato o disabilitato rimangono leggibili. Nel tema chiaro vengono
conservati il contrasto e i colori di accento appropriati.

### Soglia automatica del waterfall sui renderer GPU

Il controllo `Cut` accanto ad `Auto` ora agisce anche sul percorso GPU Direct,
oltre che su quello CPU. Il valore esistente del 10% rimane neutro e conserva
la calibrazione precedente. Valori più bassi mostrano meglio i segnali deboli
abbassando la soglia effettiva; valori più alti eliminano una parte maggiore
del rumore.

La stessa conversione viene usata dall'implementazione GPU Direct comune per
Metal, Direct3D 11, Direct3D 12 e Vulkan. OpenGL la usa quando è attivo il
percorso GPU FFT/compute; il suo fallback CPU continua a usare il calcolo
percentile originale. Anche la cronologia 3D impilata riceve la stessa soglia
attraverso le nuove righe del waterfall, rendendo il comportamento coerente
nelle viste 2D e 3D.

Nel log diagnostico vengono ora registrati la percentuale Cut selezionata e il
relativo bias in dB, così è più semplice confrontare i diversi renderer. Anche
la diagnostica CPU indica il percentile realmente utilizzato invece del vecchio
nome fisso `p25`.

### Terminologia più chiara per decoder e filtri

L'opzione precedentemente indicata come `Deep decode in TX` ora chiarisce il
suo ambito reale: `Deep decode of last RX slot during TX (list only)`. È quindi
esplicito che la funzione esamina l'ultimo slot ricevuto durante la
trasmissione e modifica soltanto la lista visualizzata.

La voce `W&P Filters Only` viene ora visualizzata correttamente invece di
mostrare il testo dell'entità HTML `W&amp;P`. La nuova terminologia del decoder
è stata riportata nelle lingue disponibili insieme alla correzione dell'etichetta
del filtro.

### Verifiche

La release è stata verificata con la suite mirata di supporto (13 test superati),
una compilazione completa di `decodium_qml`, la generazione QML e delle
traduzioni e un controllo `git diff --check` senza errori. Tutte le modifiche
locali presenti sono incluse nel commit della release.
