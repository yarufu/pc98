#!/usr/bin/env python3
"""Host-side model checks for the G98 rectangle interlace line schedule."""

from collections import Counter


LINES_PER_SIDE = 4
PERIOD = 2
STAGE_LINES = LINES_PER_SIDE * 2


def interlace_lines(height):
    stage_count = (height + STAGE_LINES - 1) // STAGE_LINES
    bottom_offset = 2 if height & 1 else 1
    stages = []

    for stage in range(stage_count):
        stage_lines = []
        stage_offset = stage * STAGE_LINES

        for line_index in range(LINES_PER_SIDE):
            relative_y = stage_offset + line_index * PERIOD
            if relative_y >= height:
                break
            stage_lines.append(relative_y)

        for line_index in range(LINES_PER_SIDE):
            relative_y = (
                height - bottom_offset - stage_offset - line_index * PERIOD
            )
            if relative_y < 0:
                break
            stage_lines.append(relative_y)

        stages.append(stage_lines)

    return stages


def check_schedule(height, expected_stages):
    stages = interlace_lines(height)
    drawn = [line for stage in stages for line in stage]
    counts = Counter(drawn)

    assert len(stages) == expected_stages
    assert len(drawn) == height
    assert min(drawn) == 0
    assert max(drawn) == height - 1
    assert set(drawn) == set(range(height))
    assert all(counts[line] == 1 for line in range(height))
    assert all(len(stage) <= STAGE_LINES for stage in stages)

    return len(drawn), sum(count - 1 for count in counts.values())


def main():
    scene_drawn, scene_duplicates = check_schedule(299, 38)
    full_drawn, full_duplicates = check_schedule(400, 50)

    plane_bytes = 80 * 400
    stage_bytes = STAGE_LINES * 80 * 4
    assert plane_bytes == 32000
    assert stage_bytes == 2560
    assert stage_bytes <= 8192

    print(
        "scene: stages=38 lines={} missing=0 duplicates={}".format(
            scene_drawn, scene_duplicates
        )
    )
    print(
        "fullscreen: stages=50 lines={} missing=0 duplicates={} "
        "out_of_range=0 stage_bytes={}".format(
            full_drawn, full_duplicates, stage_bytes
        )
    )


if __name__ == "__main__":
    main()
