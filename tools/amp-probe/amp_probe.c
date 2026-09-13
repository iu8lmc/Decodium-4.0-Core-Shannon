/* amp_probe - la telemetria dell'amplificatore e' leggibile?
 *
 * Hamlib conosce diversi amplificatori (SPE Expert, Elecraft KPA1500, Gemini)
 * e definisce le grandezze che servono al DECOMETER: potenza diretta,
 * riflessa, di picco e ROS. Ma il backend Expert NON dichiara di saperle
 * leggere, pur avendo la funzione implementata: dall'esterno e' impossibile
 * stabilire se risponda davvero.
 *
 * Questa sonda lo chiede all'apparato. Interroga ogni grandezza anche quando
 * le capacita' dichiarate dicono di no - e' proprio quello il punto - e
 * riporta cosa risponde.
 *
 * Uso:
 *   amp_probe <modello> <porta> [baud]
 *   amp_probe 401 COM7           SPE Expert 1.3K-FA / 1.5K-FA / 2K-FA
 *   amp_probe 401 /dev/ttyUSB0   lo stesso su Linux
 *   amp_probe 201 COM7           Elecraft KPA1500
 *   amp_probe 301 COM7           Gemini DX1200 / HF-1K
 *   amp_probe 1   -              simulatore Hamlib, per provare la sonda
 *
 * ATTENZIONE: una porta seriale la apre un solo programma alla volta. Chiudere
 * il software del costruttore prima di lanciare la sonda, altrimenti la porta
 * risultera' occupata.
 *
 * Compilazione:  gcc amp_probe.c -o amp_probe -lhamlib
 */

#include <hamlib/amplifier.h>
#include <stdio.h>
#include <string.h>

struct probe { setting_t bit; const char *nome; };

static const struct probe LIVELLI[] = {
    { AMP_LEVEL_PWR_FWD,       "potenza diretta   (PWR_FWD)" },
    { AMP_LEVEL_PWR_REFLECTED, "potenza riflessa  (PWR_REFLECTED)" },
    { AMP_LEVEL_PWR_PEAK,      "potenza di picco  (PWR_PEAK)" },
    { AMP_LEVEL_PWR_INPUT,     "potenza pilotante (PWR_INPUT)" },
    { AMP_LEVEL_SWR,           "ROS               (SWR)" },
};

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("uso: %s <modello> <porta> [baud]\n", argv[0]);
        printf("     401 = SPE Expert FA · 201 = Elecraft KPA1500\n");
        printf("     301 = Gemini · 1 = simulatore\n");
        return 2;
    }

    int const model = atoi(argv[1]);
    int const baud  = (argc > 3) ? atoi(argv[3]) : 0;

    rig_set_debug(RIG_DEBUG_ERR);
    amp_load_all_backends();

    AMP *amp = amp_init(model);
    if (!amp) {
        printf("modello %d sconosciuto a questa versione di Hamlib\n", model);
        return 3;
    }

    printf("Hamlib          : %s\n", hamlib_version);
    printf("amplificatore   : %s %s\n",
           amp->caps->mfg_name, amp->caps->model_name);
    printf("capacita' dichiarate: has_get_level = 0x%llx%s\n",
           (unsigned long long)amp->caps->has_get_level,
           amp->caps->has_get_level ? "" : "   (nessuna: e' il caso da chiarire)");

    if (strcmp(argv[2], "-") != 0) {
        strncpy(amp->state.ampport.pathname, argv[2], HAMLIB_FILPATHLEN - 1);
    }
    if (baud > 0) {
        amp->state.ampport.parm.serial.rate = baud;
    }

    int rc = amp_open(amp);
    printf("apertura porta  : %s\n", rc == RIG_OK ? "riuscita" : rigerror(rc));
    if (rc != RIG_OK) {
        printf("\nSe dice che la porta e' occupata: chiudere il software del\n"
               "costruttore. Una seriale la tiene un solo programma alla volta.\n");
        amp_cleanup(amp);
        return 4;
    }

    printf("\nLettura delle grandezze (interrogate comunque, anche se non dichiarate):\n");
    int leggibili = 0;
    for (size_t i = 0; i < sizeof LIVELLI / sizeof LIVELLI[0]; ++i) {
        value_t v;
        memset(&v, 0, sizeof v);
        rc = amp_get_level(amp, LIVELLI[i].bit, &v);
        if (rc == RIG_OK) {
            printf("  %-34s  RISPONDE : %.3f\n", LIVELLI[i].nome, v.f);
            leggibili++;
        } else {
            printf("  %-34s  no       : %s\n", LIVELLI[i].nome, rigerror(rc));
        }
    }

    powerstat_t st;
    if (amp_get_powerstat(amp, &st) == RIG_OK) {
        printf("\nstato alimentazione: %d\n", (int)st);
    }

    printf("\n=== ESITO ===\n");
    if (leggibili > 0) {
        printf("%d grandezze su %d rispondono: il DECOMETER PUO' mostrare i dati\n"
               "dell'amplificatore. Inviare questo esito agli sviluppatori.\n",
               leggibili, (int)(sizeof LIVELLI / sizeof LIVELLI[0]));
    } else {
        printf("Nessuna grandezza risponde su questo apparato. La lettura via\n"
               "Hamlib non e' praticabile: servirebbe implementare il protocollo\n"
               "del costruttore. Inviare comunque questo esito.\n");
    }

    amp_close(amp);
    amp_cleanup(amp);
    return leggibili > 0 ? 0 : 1;
}
