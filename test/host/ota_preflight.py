"""Exercise the SCons pre-upload action without a network or board."""
from pathlib import Path
import runpy
import socket
import tempfile
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]


class Preflight(unittest.TestCase):
    def setUp(self):
        self.env = Mock()
        self.env.subst.side_effect = lambda name: {
            '$PROJECT_DIR': str(ROOT), '$UPLOAD_PORT': '192.0.2.4', '$UPLOADER': 'espota.py'}[name]
        self.env.GetProjectOption.side_effect = lambda key, default: default
        # SCons env supports both mapping and method access.
        class Env(dict):
            pass
        env = Env(UPLOADERFLAGS=['-f', '$SOURCE'])
        for name in ('subst', 'GetProjectOption', 'Replace', 'AddPreAction'):
            setattr(env, name, getattr(self.env, name))
        self.env = env
        self.module = runpy.run_path(str(ROOT / 'scripts/ota_discover.py'),
                                    init_globals={'env': env, 'Import': lambda _: None})
        self.action = self.module['discover_pico_ip']
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.image = Path(self.tmp.name) / 'firmware.bin.signed'
        self.image.write_bytes(b'x' * 100)
        self.sock = Mock()
        self.sock.recvfrom.return_value = (b'OTA_READY', ('192.0.2.4', 44444))
        self.sockets = patch('socket.socket', return_value=self.sock).start()
        patch('socket.gethostbyname', return_value='192.0.2.4').start()
        patch('time.sleep').start()
        self.addCleanup(patch.stopall)

    def run_action(self):
        self.action([str(self.image)], [], self.env)

    def test_size_boundary(self):
        from ota_limits import staging_limit
        limit = staging_limit('1m')
        self.assertEqual(limit, 983040)
        self.image.write_bytes(b'x' * limit)
        self.run_action()
        self.sockets.reset_mock()
        self.image.write_bytes(b'x' * (limit + 1))
        with self.assertRaises(ValueError): self.run_action()
        self.sockets.assert_not_called()  # No PREP before rejection.

    def test_preferred_signed_artifact(self):
        signed = Path(str(self.image) + '.signed')
        signed.write_bytes(b'x' * 983041)
        with self.assertRaises(ValueError): self.run_action()
        self.sockets.assert_not_called()

    def test_empty_and_compressed(self):
        for data in (b'', b'\x1f\x8b' + b'x'*100):
            self.image.write_bytes(data)
            with self.assertRaises(ValueError): self.run_action()
        self.sockets.assert_not_called()

    def test_wrong_peer(self):
        self.sock.recvfrom.return_value = (b'OTA_READY', ('192.0.2.99', 44444))
        with self.assertRaises(RuntimeError): self.run_action()
        self.assertEqual(self.sock.sendto.call_count, 5)

    def test_timeout(self):
        self.sock.recvfrom.side_effect = socket.timeout
        with self.assertRaises(RuntimeError): self.run_action()
        self.sock.close.assert_called_once()

    def test_success(self):
        self.run_action()
        self.assertEqual(self.sock.sendto.call_count, 1)
        self.env.Replace.assert_any_call(UPLOAD_PORT='192.0.2.4')


if __name__ == '__main__':
    unittest.main()
