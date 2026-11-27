"""Rebuild the previous scalar min-sum branch within the current source."""
import subprocess


def previous_minsum(source, root):
    marker = '      else\n        {\n          // Each check row is read once.'
    if marker not in source:
        return source
    old = subprocess.check_output(['git', 'show', 'HEAD:Detector/FtxLdpc.cpp'], cwd=root, text=True)
    start_old = old.index('      else\n        {\n          for (int bit = 0; bit < kLdpc17491N; ++bit)', old.index('if (use_exact_bp)'))
    end_old = old.index('\n  for (int i = 0; i < nosd; ++i)', start_old)
    start = source.index(marker, source.index('if (use_exact_bp)'))
    end = source.index('\n  for (int i = 0; i < nosd; ++i)', start)
    return source[:start] + old[start_old:end_old] + source[end:]


def candidate_minsum(source, root):
    reference = previous_minsum(source, root)
    lab = (root / 'tests/tools/ldpc_minsum_weak_order_lab.py').read_text()
    prototype = lab.split("prototype='''", 1)[1].split("'''", 1)[0]
    start = reference.index('      else\n        {\n          for (int bit = 0; bit < kLdpc17491N; ++bit)',
                            reference.index('if (use_exact_bp)'))
    end = reference.index('\n  for (int i = 0; i < nosd; ++i)', start)
    return reference[:start] + prototype + reference[end:]
