#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import threading
from pathlib import Path
from typing import Dict, Optional

from ament_index_python.packages import PackageNotFoundError, get_package_share_directory
import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from std_msgs.msg import String


TOPIC_NAME = 'ui_call_script_cmd'
MAX_LOG_SNIPPET_CHARS = 400
ALLOWED_SCRIPT_NAMES = (
    'test.sh',
    'triger_stop_capture.sh',
    'end_pipe_postprocess.sh',
)


class UiCallShNode(Node):
    def __init__(self) -> None:
        super().__init__('ui_call_sh_node')
        self._process_lock = threading.Lock()
        self._active_process: Optional[subprocess.Popen[str]] = None
        self._active_script_name: Optional[str] = None
        self._script_paths = self._build_script_map()
        self._log_script_availability()
        self.create_subscription(String, TOPIC_NAME, self._handle_script_command, 10)
        self.get_logger().info(
            f'ui_call_sh_node started, listening on topic "{TOPIC_NAME}"'
        )

    def _build_script_map(self) -> Dict[str, Path]:
        scripts_dir = self._resolve_scripts_dir()
        return {script_name: scripts_dir / script_name for script_name in ALLOWED_SCRIPT_NAMES}

    def _resolve_scripts_dir(self) -> Path:
        try:
            package_share_dir = Path(get_package_share_directory('ui_call_sh'))
            installed_scripts_dir = package_share_dir / 'scripts'
            if installed_scripts_dir.exists():
                return installed_scripts_dir
        except PackageNotFoundError:
            pass

        return Path(__file__).resolve().parents[1] / 'scripts'

    def _log_script_availability(self) -> None:
        for script_name, script_path in self._script_paths.items():
            if not script_path.exists():
                self.get_logger().error(
                    f'Whitelisted script missing: {script_name} ({script_path})'
                )
                continue
            if not os.access(script_path, os.X_OK):
                self.get_logger().error(
                    f'Whitelisted script is not executable: {script_name} ({script_path})'
                )
                continue
            self.get_logger().info(f'Whitelisted script ready: {script_name} ({script_path})')

    def _handle_script_command(self, msg: String) -> None:
        script_name = msg.data.strip()
        if not script_name:
            self.get_logger().warning('Received empty ui_call_script_cmd message, ignoring')
            return

        script_path = self._script_paths.get(script_name)
        if script_path is None:
            self.get_logger().warning(f'Received unknown script command "{script_name}", ignoring')
            return

        with self._process_lock:
            if self._active_process and self._active_process.poll() is None:
                self.get_logger().warning(
                    f'Script "{self._active_script_name}" is still running, '
                    f'rejecting new command "{script_name}"'
                )
                return

            if not script_path.exists():
                self.get_logger().error(f'Script not found: {script_path}')
                return

            if not os.access(script_path, os.X_OK):
                self.get_logger().error(f'Script is not executable: {script_path}')
                return

            try:
                process = subprocess.Popen(
                    [str(script_path)],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                )
            except Exception as error:  # pragma: no cover - defensive runtime guard
                self.get_logger().error(f'Failed to start script "{script_name}": {error}')
                return

            self._active_process = process
            self._active_script_name = script_name

        self.get_logger().info(f'Started script "{script_name}" with pid {process.pid}')
        watcher = threading.Thread(
            target=self._wait_for_process,
            args=(script_name, script_path, process),
            daemon=True,
        )
        watcher.start()

    def _wait_for_process(
        self,
        script_name: str,
        script_path: Path,
        process: subprocess.Popen[str],
    ) -> None:
        stdout_data, stderr_data = process.communicate()
        exit_code = process.returncode

        stdout_snippet = self._format_output(stdout_data)
        stderr_snippet = self._format_output(stderr_data)

        if stdout_snippet:
            self.get_logger().info(f'Script "{script_name}" stdout: {stdout_snippet}')
        if stderr_snippet:
            self.get_logger().warning(f'Script "{script_name}" stderr: {stderr_snippet}')

        if exit_code == 0:
            self.get_logger().info(
                f'Script "{script_name}" finished successfully with exit code 0 ({script_path})'
            )
        else:
            self.get_logger().error(
                f'Script "{script_name}" failed with exit code {exit_code} ({script_path})'
            )

        with self._process_lock:
            if self._active_process is process:
                self._active_process = None
                self._active_script_name = None

    def destroy_node(self) -> bool:
        with self._process_lock:
            active_process = self._active_process
            active_script_name = self._active_script_name

        if active_process and active_process.poll() is None:
            self.get_logger().warning(
                f'Node shutdown requested while script "{active_script_name}" is still running'
            )
        return super().destroy_node()

    @staticmethod
    def _format_output(output: Optional[str]) -> str:
        if not output:
            return ''
        normalized = ' '.join(output.split())
        if len(normalized) <= MAX_LOG_SNIPPET_CHARS:
            return normalized
        return f'{normalized[:MAX_LOG_SNIPPET_CHARS]}...'


def main(args: Optional[list[str]] = None) -> None:
    rclpy.init(args=args)
    node = UiCallShNode()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()
