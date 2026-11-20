#ifndef LEGACY_DECODE_WINDOW_HPP
#define LEGACY_DECODE_WINDOW_HPP

namespace decodium
{
namespace legacy
{

// The QML decode panes retain at most 250 rows. Keep a small margin for rows
// removed by live filters without repeatedly parsing the full widget history.
static constexpr int kLiveDecodeSnapshotRows = 384;

inline int recent_decode_window_start(int totalRows,
                                      int maxRows = kLiveDecodeSnapshotRows) noexcept
{
    if (totalRows <= 0) {
        return 0;
    }
    if (maxRows <= 0) {
        return totalRows;
    }
    return totalRows > maxRows ? totalRows - maxRows : 0;
}

}

}

#endif // LEGACY_DECODE_WINDOW_HPP
