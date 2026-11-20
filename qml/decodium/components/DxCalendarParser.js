.pragma library

// DXCAL follows the announced NG3K end date as inclusive, including for
// all-day events. Keep this source-specific convention in the presentation.
function unescapeText(value) {
    return value.replace(/\\[nN]/g, "\n").replace(/\\,/g, ",")
                .replace(/\\;/g, ";").replace(/\\\\/g, "\\");
}
function isoDate(value) {
    var match = /^(\d{4})(\d{2})(\d{2})(?:T\d{6}Z?)?$/.exec(value.trim());
    if (!match) return "";
    var d = new Date(Date.UTC(+match[1], +match[2] - 1, +match[3]));
    if (d.getUTCFullYear() !== +match[1] || d.getUTCMonth() !== +match[2] - 1 || d.getUTCDate() !== +match[3]) return "";
    return match[1] + "-" + match[2] + "-" + match[3];
}
function parse(text) {
    if (typeof text !== "string" || text.length > 2000000 || !/^BEGIN:VCALENDAR\s*$/m.test(text) || !/^END:VCALENDAR\s*$/m.test(text)) return [];
    var lines = text.replace(/\r\n/g, "\n").replace(/\r/g, "\n").replace(/\n[ \t]/g, "").split("\n");
    var entries = [], event = null;
    for (var i = 0; i < lines.length; ++i) {
        var line = lines[i];
        if (line === "BEGIN:VEVENT") { event = {}; continue; }
        if (line === "END:VEVENT") {
            if (event && event.start && event.end && event.start <= event.end && event.title) {
                var m = /^(.*?)\s*\(([^()]*)\)\s*$/.exec(event.title);
                event.entity = m ? m[1].trim() : event.title;
                event.call = m ? m[2].trim().toUpperCase() : "";
                event.description = event.description || "";
                entries.push(event);
            }
            event = null;
            continue;
        }
        if (!event) continue;
        var colon = line.indexOf(":");
        if (colon < 0) continue;
        var key = line.slice(0, colon).split(";")[0].toUpperCase();
        var value = line.slice(colon + 1);
        if (key === "SUMMARY") event.title = unescapeText(value).trim();
        else if (key === "DESCRIPTION") event.description = unescapeText(value).trim();
        else if (key === "DTSTART") event.start = isoDate(value);
        else if (key === "DTEND") event.end = isoDate(value);
    }
    entries.sort(function(a, b) { return a.start.localeCompare(b.start) || a.title.localeCompare(b.title); });
    return entries;
}
function filter(entries, query, selection, today) {
    var needle = String(query || "").trim().toLocaleLowerCase();
    return entries.filter(function(e) {
        var status = today < e.start ? "upcoming" : today > e.end ? "expired" : "active";
        if (selection !== "all" && selection !== status) return false;
        return !needle || (e.title + " " + e.description).toLocaleLowerCase().indexOf(needle) >= 0;
    });
}
