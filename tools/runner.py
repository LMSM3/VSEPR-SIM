"""
Runner - Subprocess wrapper with environment control and logging
"""
import subprocess
import sys
from pathlib import Path
from datetime import datetime
from typing import Optional, List, Dict
from . import config

class RunResult:
    """Result from running a command"""
    def __init__(self, returncode: int, stdout: str, stderr: str, 
                 command: str, duration: float):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.command = command
        self.duration = duration
        self.success = returncode == 0
    
    def __bool__(self):
        return self.success

class Runner:
    """Execute commands and manage outputs"""
    
    def __init__(self, log_name: str, verbose: bool = False):
        self.log_name = log_name
        self.verbose = verbose
        self.log_file = config.get_today_log_dir() / f"{log_name}.log"
        
        # Initialize log
        with open(self.log_file, 'w') as f:
            f.write(f"=== {log_name} started at {datetime.now()} ===\n\n")
    
    def run(self, cmd: List[str], cwd: Optional[Path] = None, 
            env: Optional[Dict] = None, capture: bool = True) -> RunResult:
        """
        Run a command and capture/log output
        
        Args:
            cmd: Command and arguments as list
            cwd: Working directory
            env: Environment variables (merged with defaults)
            capture: Whether to capture output
        """
        start = datetime.now()
        
        # Merge environment
        full_env = dict(config.ENV_DEFAULTS)
        if env:
            full_env.update(env)
        
        # Log command
        cmd_str = ' '.join(str(c) for c in cmd)
        self._log(f"CMD: {cmd_str}")
        if cwd:
            self._log(f"CWD: {cwd}")
        
        # Print to console
        if self.verbose:
            print(f"{config.ARROW} {cmd_str}")
        
        try:
            # Run command
            result = subprocess.run(
                cmd,
                cwd=cwd,
                env=full_env,
                capture_output=capture,
                text=True,
                timeout=300  # 5 minute timeout
            )
            
            duration = (datetime.now() - start).total_seconds()
            
            # Log output
            if result.stdout:
                self._log(f"STDOUT:\n{result.stdout}")
            if result.stderr:
                self._log(f"STDERR:\n{result.stderr}")
            
            self._log(f"EXIT: {result.returncode} ({duration:.2f}s)\n")
            
            # Create result
            run_result = RunResult(
                result.returncode,
                result.stdout,
                result.stderr,
                cmd_str,
                duration
            )
            
            # Print status
            if run_result.success:
                print(f"{config.CHECKMARK} {self.log_name} ({duration:.1f}s)")
            else:
                print(f"{config.CROSSMARK} {self.log_name} failed (exit {result.returncode})")
                if not self.verbose and result.stderr:
                    print(f"  Error: {result.stderr[:200]}")
            
            return run_result
            
        except subprocess.TimeoutExpired:
            duration = (datetime.now() - start).total_seconds()
            self._log(f"TIMEOUT after {duration}s\n")
            print(f"{config.CROSSMARK} {self.log_name} timeout")
            return RunResult(-1, "", "Timeout", cmd_str, duration)
        
        except Exception as e:
            duration = (datetime.now() - start).total_seconds()
            self._log(f"ERROR: {e}\n")
            print(f"{config.CROSSMARK} {self.log_name} error: {e}")
            return RunResult(-1, "", str(e), cmd_str, duration)
    
    def bash(self, script: str, **kwargs) -> RunResult:
        """Run a bash command"""
        return self.run(["bash", "-c", script], **kwargs)
    
    def python(self, script: Path, args: List[str] = None, **kwargs) -> RunResult:
        """Run a Python script"""
        cmd = [sys.executable, str(script)]
        if args:
            cmd.extend(args)
        return self.run(cmd, **kwargs)
    
    def _log(self, message: str):
        """Write to log file"""
        with open(self.log_file, 'a') as f:
            timestamp = datetime.now().strftime(config.LOG_TIME_FORMAT)
            f.write(f"[{timestamp}] {message}\n")
