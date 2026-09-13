/* Client Hamlib NET rigctl (modello 2): la prova definitiva del formato. */
#include <hamlib/rig.h>
#include <stdio.h>
int main(int argc, char **argv) {
  rig_set_debug(RIG_DEBUG_ERR);
  RIG *r = rig_init(2);                 /* NET rigctl */
  if (!r) { printf("rig_init fallita\n"); return 1; }
  strncpy(r->state.rigport.pathname, argv[1], HAMLIB_FILPATHLEN - 1);
  int rc = rig_open(r);
  printf("rig_open        : %d %s\n", rc, rigerror(rc));
  if (rc != RIG_OK) return 2;
  freq_t f = 0; rmode_t m; pbwidth_t w; ptt_t p; split_t s; vfo_t tx;
  rc = rig_get_freq(r, RIG_VFO_CURR, &f);
  printf("rig_get_freq    : %d  %.0f Hz\n", rc, f);
  rc = rig_get_mode(r, RIG_VFO_CURR, &m, &w);
  printf("rig_get_mode    : %d  %s / %d Hz\n", rc, rig_strrmode(m), (int)w);
  rc = rig_get_ptt(r, RIG_VFO_CURR, &p);
  printf("rig_get_ptt     : %d  %d\n", rc, (int)p);
  rc = rig_get_split_vfo(r, RIG_VFO_CURR, &s, &tx);
  printf("rig_get_split   : %d  split=%d\n", rc, (int)s);
  rc = rig_set_freq(r, RIG_VFO_CURR, 14076000.0);
  printf("rig_set_freq    : %d\n", rc);
  rig_close(r); rig_cleanup(r);
  return 0;
}
