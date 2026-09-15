"""Local computer-use CLI for D2R. Inspect a screenshot before sending input."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import sys
import time

import capture
import controls
import win32 as w


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, help='Required for all actions except list')
    parser.add_argument('--output-dir', type=Path, default=Path(os.environ['LOCALAPPDATA']) / 'D2RAccess' / 'computer-use')
    sub = parser.add_subparsers(dest='action', required=True)
    for action in ['list', 'focus', 'capture']:
        sub.add_parser(action)
    key = sub.add_parser('key')
    key.add_argument('keys', help='For example ctrl+t, w, or w+d')
    key.add_argument('--seconds', type=float, default=0.08)
    mouse = sub.add_parser('click')
    mouse.add_argument('x', type=int)
    mouse.add_argument('y', type=int)
    mouse.add_argument('--button', choices=['left', 'right'], default='left')
    move = sub.add_parser('move')
    move.add_argument('x', type=int)
    move.add_argument('y', type=int)
    wheel = sub.add_parser('scroll')
    wheel.add_argument('x', type=int)
    wheel.add_argument('y', type=int)
    wheel.add_argument('steps', type=int)
    args = parser.parse_args()
    if args.action != 'list' and args.pid is None:
        parser.error('--pid is required for a targeted action')
    result = dict(time=datetime.now(timezone.utc).isoformat(), action=args.action)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    try:
        if args.action == 'list':
            result['windows'] = w.game_windows()
        else:
            info = w.target(args.pid)
            result['window'] = info
            if args.action == 'focus':
                if info['minimized']:
                    w.show_window(info['hwnd'], 9)  # SW_RESTORE
                w.set_foreground(info['hwnd'])
                time.sleep(0.15)
                result['window'] = w.require_foreground(info)
            elif args.action == 'capture':
                stamp = datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
                result['capture'] = capture.screenshot(info, args.output_dir / f'{stamp}.png')
            elif args.action == 'key':
                result.update(keys=args.keys, seconds=args.seconds)
                controls.press_keys(info, args.keys, args.seconds)
            elif args.action == 'click':
                result.update(x=args.x, y=args.y, button=args.button)
                controls.click(info, args.x, args.y, args.button)
            elif args.action == 'move':
                result.update(x=args.x, y=args.y)
                controls.move_mouse(info, args.x, args.y)
            elif args.action == 'scroll':
                result.update(x=args.x, y=args.y, steps=args.steps)
                controls.scroll(info, args.x, args.y, args.steps)
        result['ok'] = True
    except (OSError, RuntimeError, ValueError) as error:
        result.update(ok=False, error=str(error))
    with (args.output_dir / 'actions.jsonl').open('a', encoding='utf-8') as stream:
        stream.write(json.dumps(result, ensure_ascii=False) + '\n')
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    sys.exit(main())
