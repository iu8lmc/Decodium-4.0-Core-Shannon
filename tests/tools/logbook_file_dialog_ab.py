#!/usr/bin/env python3
"""A/B the real file-dialog helper with a detached topmost QQuickWindow.

Requires Qt6 Widgets, Quick, Test and pkg-config. Builds disposable files only.
QtTest injects a click; an offscreen run does not establish native OS z-order.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
REL = 'src/bridge/DecodiumBridgeReceivedFiles.cpp'

def source(revision):
    if revision == 'before':
        return subprocess.check_output(['git', 'show', f'HEAD:{REL}'], cwd=ROOT, text=True)
    return (ROOT / REL).read_text()

def fragment(text):
    path_start = text.index('static QString localDialogPath(')
    path_end = text.index('static QString safeFt2LinkReceivedDirectoryName(', path_start)
    start = text.index('static QString dialogFilterString(')
    end = text.index('QString DecodiumBridge::saveFileDialog(', start)
    return (text[path_start:path_end] + text[start:end]).replace(
        'QString DecodiumBridge::openFileDialog(', 'QString testOpenFileDialog(').replace(
        'QString DecodiumBridge::openFileDialogForWindow(', 'QString testOpenFileDialogForWindow(').replace(
        'return openFileDialog(title, initialPath, nameFilters);', 'return testOpenFileDialog(title, initialPath, nameFilters);').replace(
        'const QStringList& nameFilters) const\n{', 'const QStringList& nameFilters)\n{').replace(
        'QObject* owner) const\n{', 'QObject* owner)\n{')

HEADERS = '''#include <QApplication>\n#include <QQuickWindow>\n#include <QWindow>\n#include <QFileDialog>\n#include <QDialogButtonBox>\n#include <QPushButton>\n#include <QLineEdit>\n#include <QDir>\n#include <QUrl>\n#include <QTimer>\n#include <QTest>\n#include <cstdio>\n'''
MAIN = '''
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  QWidget other;
  other.resize(200, 100);
  other.show();
  QQuickWindow log;
  log.setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  log.resize(600, 400);
  log.show();
  log.requestActivate();
  QTest::qWait(300);
  const bool select = argc > 1;
  bool observed=false, owned=false, topmost=false, click=false;
  QTimer::singleShot(500, &app, [&] {
    for (QWidget *w : QApplication::topLevelWidgets()) {
      auto *d = qobject_cast<QFileDialog *>(w);
      if (!d || !d->isVisible()) continue;
      observed = true;
      owned = d->windowHandle() && d->windowHandle()->transientParent() == &log;
      topmost = d->windowFlags().testFlag(Qt::WindowStaysOnTopHint);
      auto *box = d->findChild<QDialogButtonBox *>();
      if (select) {
        if (auto *edit = d->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"))) {
          edit->setText(QString::fromLocal8Bit(argv[1]));
          click = QMetaObject::invokeMethod(d, "accept");
        } else d->reject();
        break;
      }
      auto *button = box ? box->button(QDialogButtonBox::Cancel) : nullptr;
      if (button && button->isVisible() && button->isEnabled()) {
        QTest::mouseClick(button, Qt::LeftButton);
        click = true;
      } else {
        d->reject();
      }
      break;
    }
  });
  QTimer::singleShot(5000, &app, [&] {
    for (QWidget *w : QApplication::topLevelWidgets())
      if (auto *d=qobject_cast<QFileDialog *>(w)) d->reject();
  });
  auto result = DIALOG_CALL;
  const bool correct = select ? result == QString::fromLocal8Bit(argv[1]) : result.isEmpty();
  std::printf("observed=%d owned_by_log=%d dialog_topmost=%d action_sent=%d result_correct=%d\\n",
              observed, owned, topmost, click, correct);
  return observed && click && correct ? 0 : 1;
}
'''

def main():
    pkg = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Widgets', 'Qt6Quick', 'Qt6Test'], text=True)
    with tempfile.TemporaryDirectory(prefix='decodium-log-dialog-') as folder:
        tmp = Path(folder)
        adif = tmp / 'sample.adi'
        adif.write_text('<EOH>\n<CALL:5>TEST1<EOR>\n')
        for name in ('before', 'after'):
            cpp = tmp / (name + '.cpp')
            call = ('testOpenFileDialog(QStringLiteral("Import ADIF"), QString(), {})'
                    if name == 'before' else
                    'testOpenFileDialogForWindow(QStringLiteral("Import ADIF"), QString(), {}, &log)')
            cpp.write_text(HEADERS + fragment(source(name)) + MAIN.replace('DIALOG_CALL', call))
            exe = tmp / name
            subprocess.run(['c++', '-std=c++17', '-fPIC', str(cpp), '-o', str(exe), *shlex.split(pkg)], check=True)
            env = dict(os.environ)
            env.setdefault('QT_QPA_PLATFORM', 'offscreen')
            for action, args in (('cancel', []), ('select', [str(adif)])):
                p = subprocess.run([str(exe), *args], env=env, text=True, capture_output=True, timeout=15)
                print(f'{name} {action}: ' + p.stdout.strip(), flush=True)
                if p.returncode:
                    print(p.stderr[-3000:])
                    raise SystemExit(f'{name} {action} exited {p.returncode}')
                if name == 'after' and ('owned_by_log=1' not in p.stdout
                                        or 'dialog_topmost=1' not in p.stdout):
                    raise SystemExit('updated dialog did not inherit the explicit topmost owner')

if __name__ == '__main__':
    main()
