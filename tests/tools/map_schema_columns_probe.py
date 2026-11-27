#!/usr/bin/env python3
"""Compare real old/new column migrations on isolated SQLite databases."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
rel = 'src/services/MapIntelligenceService.cpp'
old = subprocess.check_output(['git', 'show', 'HEAD:' + rel], cwd=root, text=True)
new = (root / rel).read_text()
def section(source, start, end):
    return source[source.index(start):source.index(end, source.index(start))]
old_fn = section(old, 'bool ensureColumn(', '\nQString transmittingCallFromMessage')
new_fn = section(new, 'bool ensureColumn(', '\nQString transmittingCallFromMessage')
old_migrations = section(old, '    struct ColumnMigration {', '\n    for (ColumnMigration const& migration')
new_migrations = section(new, '    struct ColumnMigration {', '\n    QHash<QString, QSet<QString>> knownColumns;')
if old_migrations != new_migrations:
    raise RuntimeError('migration list changed unexpectedly')
common = section(new, 'bool execSql(', '\nbool ensureColumn(')
program = r'''
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QVariant>
#include <memory>
#include <utility>
COMMON
OLD
NEW
MIGRATIONS
bool apply(QSqlDatabase& db, bool optimized) {
    QHash<QString, QSet<QString>> cache;
    QString error;
    for (const auto& m : migrations) {
        QString t=QString::fromLatin1(m.table), c=QString::fromLatin1(m.column), d=QString::fromLatin1(m.definition);
        if (!(optimized ? ensureNew(db,t,c,d,cache,&error) : ensureOld(db,t,c,d,&error))) {
            QTextStream(stderr) << error << Qt::endl; return false;
        }
    }
    return true;
}
QString schema(QSqlDatabase& db) {
    QSqlQuery query(db); QString out;
    if (!query.exec("SELECT type,name,tbl_name,sql FROM sqlite_master ORDER BY type,name")) return {};
    while(query.next()) for(int i=0;i<4;++i) out+=query.value(i).toString()+"|";
    return out;
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    auto a=QSqlDatabase::addDatabase("QSQLITE","old"); a.setDatabaseName(":memory:");
    auto b=QSqlDatabase::addDatabase("QSQLITE","new"); b.setDatabaseName(":memory:");
    if(!a.open() || !b.open()) return 1;
    for(auto* db : {&a,&b}) {
        QSqlQuery q(*db);
        for(const char* sql : {"CREATE TABLE map_qso(id INTEGER PRIMARY KEY, satellite TEXT, sat_mode TEXT, freq_rx_mhz REAL)",
                              "CREATE TABLE map_spot(id INTEGER PRIMARY KEY, grid_origin TEXT)",
                              "CREATE TABLE map_spot_event(id INTEGER PRIMARY KEY)",
                              "INSERT INTO map_qso(id,satellite) VALUES(7,'TEST')"})
            if(!q.exec(sql)) return 2;
    }
    if(!apply(a,false) || !apply(b,true) || schema(a)!=schema(b)) return 3;
    QSqlQuery check(b); if(!check.exec("SELECT satellite FROM map_qso WHERE id=7") || !check.next() || check.value(0).toString()!="TEST") return 4;
    constexpr int iterations=200;
    QElapsedTimer timer; timer.start();
    for(int i=0;i<iterations;++i) if(!apply(a,false)) return 5;
    qint64 before=timer.nsecsElapsed(); timer.restart();
    for(int i=0;i<iterations;++i) if(!apply(b,true)) return 6;
    qint64 after=timer.nsecsElapsed();
    QTextStream(stdout)<<"iterations="<<iterations<<" before_ms="<<before/1000000.0<<" after_ms="<<after/1000000.0<<Qt::endl;
}
'''
program = program.replace('COMMON', common).replace('OLD', old_fn.replace('ensureColumn(', 'ensureOld(', 1)).replace('NEW', new_fn.replace('ensureColumn(', 'ensureNew(', 1)).replace('MIGRATIONS', old_migrations.replace('    ', '', 1))
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core', 'Qt6Sql'], text=True))
with tempfile.TemporaryDirectory(prefix='decodium-map-columns-') as tmp:
    source = Path(tmp) / 'probe.cpp'
    executable = Path(tmp) / 'probe'
    source.write_text(program)
    subprocess.run([*shlex.split(os.environ.get('CXX', 'c++')), '-O2', '-std=c++17', str(source), *flags, '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
