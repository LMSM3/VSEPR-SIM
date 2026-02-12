#!/usr/bin/env python3
"""
Flower - VSEPR-Sim Orchestrator CLI

The single entrypoint for all VSEPR-Sim operations.
Everything is a verb. Every verb does one thing well.

Philosophy:
    - Python owns orchestration, not physics
    - C++ binaries are black boxes we call
    - One command = one outcome
    - Logs go to logs/, outputs go to out/
    - Nonzero exit = failure
    - ✓/✗ for humans

Usage:
    python -m tools.flower build
    python -m tools.flower run H2O --xyz
    python -m tools.flower viz H2O --mode cartoon
    python -m tools.flower export H2O --formats xyz html
    python -m tools.flower report H2O --pdf
    python -m tools.flower clean
"""

import sys
import argparse
from pathlib import Path

# Add parent to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from tools import targets, config

def main():
    parser = argparse.ArgumentParser(
        description="VSEPR-Sim Orchestrator - One command, one outcome",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    %(prog)s build                      # Build C++ binaries
    %(prog)s test                       # Run all tests
    %(prog)s run H2O --xyz              # Build water molecule
    %(prog)s viz H2O                    # Generate HTML viewer
    %(prog)s export H2O                 # Export all formats
    %(prog)s report H2O                 # Package results
    %(prog)s clean                      # Clean all artifacts

Logs: logs/YYYY-MM-DD/<task>.log
Outputs: out/
        """
    )
    
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Verbose output')
    
    subparsers = parser.add_subparsers(dest='verb', help='Command to run')
    
    # BUILD
    p_build = subparsers.add_parser('build', help='Build C++ binaries')
    p_build.add_argument('--clean', action='store_true', 
                        help='Clean before building')
    
    # TEST
    p_test = subparsers.add_parser('test', help='Run tests')
    p_test.add_argument('pattern', nargs='?', default='*',
                       help='Test pattern (default: all)')
    
    # RUN (molecule builder)
    p_run = subparsers.add_parser('run', help='Build molecule')
    p_run.add_argument('formula', help='Chemical formula (e.g., H2O)')
    p_run.add_argument('--no-optimize', action='store_true',
                      help='Skip geometry optimization')
    p_run.add_argument('--xyz', action='store_true',
                      help='Export XYZ file')
    
    # VIZ
    p_viz = subparsers.add_parser('viz', help='Generate visualization')
    p_viz.add_argument('molecule', help='Molecule name or XYZ file')
    p_viz.add_argument('--mode', default='default',
                      help='Rendering mode')
    p_viz.add_argument('--no-open', action='store_true',
                      help="Don't auto-open browser")
    
    # EXPORT
    p_export = subparsers.add_parser('export', help='Export molecule data')
    p_export.add_argument('molecule', help='Molecule name')
    p_export.add_argument('--formats', nargs='+', 
                         default=['xyz', 'html'],
                         help='Export formats (xyz, html, json, png)')
    
    # REPORT
    p_report = subparsers.add_parser('report', help='Generate analysis report')
    p_report.add_argument('molecule', help='Molecule name')
    p_report.add_argument('--format', default='html',
                         choices=['html', 'pdf', 'markdown'],
                         help='Report format')
    
    # CLEAN
    p_clean = subparsers.add_parser('clean', help='Clean artifacts')
    p_clean.add_argument('target', nargs='?', default='all',
                        choices=['all', 'build', 'out', 'logs'],
                        help='What to clean')
    
    # Parse
    args = parser.parse_args()
    
    if not args.verb:
        parser.print_help()
        return 1
    
    # Ensure directories exist
    config.ensure_dirs()
    
    # Dispatch to verb
    try:
        if args.verb == 'build':
            success = targets.build(
                verbose=args.verbose,
                clean=args.clean
            )
        
        elif args.verb == 'test':
            success = targets.test(
                pattern=args.pattern,
                verbose=args.verbose
            )
        
        elif args.verb == 'run':
            result = targets.run_molecule(
                formula=args.formula,
                optimize=not args.no_optimize,
                xyz=args.xyz,
                verbose=args.verbose
            )
            success = result is not None
        
        elif args.verb == 'viz':
            result = targets.viz(
                molecule=args.molecule,
                mode=args.mode,
                open_browser=not args.no_open,
                verbose=args.verbose
            )
            success = result is not None
        
        elif args.verb == 'export':
            success = targets.export(
                molecule=args.molecule,
                formats=args.formats,
                verbose=args.verbose
            )
        
        elif args.verb == 'report':
            result = targets.report(
                molecule=args.molecule,
                format=args.format,
                verbose=args.verbose
            )
            success = result is not None
        
        elif args.verb == 'clean':
            success = targets.clean(
                target=args.target,
                verbose=args.verbose
            )
        
        else:
            print(f"{config.CROSSMARK} Unknown verb: {args.verb}")
            return 1
        
        return 0 if success else 1
    
    except KeyboardInterrupt:
        print(f"\n{config.CROSSMARK} Interrupted")
        return 130
    
    except Exception as e:
        print(f"{config.CROSSMARK} Error: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        return 1

if __name__ == '__main__':
    sys.exit(main())
