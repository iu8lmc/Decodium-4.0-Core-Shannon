#!/usr/bin/env python3
# Scores decoder outputs written by ft8_bench_run.sh against the .truth files of ft8_bench_corpus.py.
# A decode that is not in the truth list counts as false.
# usage: ft8_bench_score.py <corpdir> <profile> <outA> <outB> ...   hits/false/time on the files present in ALL outdirs, per kind
import sys, os, glob, re, collections
corp, prof = sys.argv[1], sys.argv[2]; outs = sys.argv[3:]
files = [os.path.basename(t)[:-6] for t in sorted(glob.glob(corp + "/*.truth"))]
files = [n for n in files if all(os.path.exists("%s/%s.%s.time" % (o, n, prof)) for o in outs)]
kinds = sorted(set(n.split("_")[0] for n in files))
for k in kinds:
    for o in outs:
        hit = collections.Counter(); tot = collections.Counter(); false = 0; t = 0.0; nf = 0
        for n in files:
            if not n.startswith(k + "_"): continue
            nf += 1; t += float(open("%s/%s.%s.time" % (o, n, prof)).read())
            dec = set(re.search(r'decoded="(.*)"', l).group(1).strip() for l in open("%s/%s.%s.txt" % (o, n, prof)) if 'decoded="' in l)
            truth = {}
            for l in open("%s/%s.truth" % (corp, n)):
                a = l.split("|")[0].split(None, 3); truth[a[3].strip()] = round(float(a[1]))
            for m, s in truth.items(): tot[s] += 1; hit[s] += m in dec
            false += sum(1 for m in dec if m not in truth)
        if k == "qrm":
            w = [s for s in tot if s <= -17]; per = "weak:%d/%d strong:%d/%d" % (sum(hit[s] for s in w), sum(tot[s] for s in w), sum(hit[s] for s in tot if s > -17), sum(tot[s] for s in tot if s > -17))
        else: per = " ".join("%d:%d/%d" % (s, hit[s], tot[s]) for s in sorted(tot))
        print("%-6s %-22s files=%3d time=%7.1fs hits=%4d/%-4d false=%d  %s" % (k, os.path.basename(o), nf, t, sum(hit.values()), sum(tot.values()), false, per))
