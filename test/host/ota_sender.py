"""Exercise the actual vendored sender's TCP result handling without a board."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('cabinet_espota', ROOT / 'lib/cabinet_ota/tools/cabinet_espota.py')
sender = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sender)


class SenderResult(unittest.TestCase):
    def transfer(self, replies):
        connection = Mock()
        # Exhausted replies deliberately raise: the sender must finish before that.
        connection.recv.side_effect = replies
        listener = Mock()
        listener.accept.return_value = (connection, ('192.0.2.4', 12345))
        invitation = Mock()
        invitation.recv.return_value = b'OK'
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / 'firmware.bin'
            image.write_bytes(b'x' * 100)
            with patch.object(sender.socket, 'socket', side_effect=[listener, invitation]):
                result = sender.serve('192.0.2.4', '0.0.0.0', 2040, 12345, '', str(image))
        connection.close.assert_called()
        return result, connection.recv.call_count

    def test_early_final_ok(self):
        self.assertEqual(self.transfer([b'100OK']), (0, 1))

    def test_late_final_ok(self):
        self.assertEqual(self.transfer([b'100', b'OK']), (0, 2))

    def test_closed_without_result(self):
        self.assertEqual(self.transfer([b'100', b'']), (1, 2))

    def test_split_ok(self):
        self.assertEqual(self.transfer([b'100O', b'K']), (0, 2))

    def test_early_error_is_not_ok(self):
        self.assertEqual(self.transfer([b'ERROR[7]: MD5 failure']), (1, 1))

    def test_split_error_is_not_ok(self):
        self.assertEqual(self.transfer([b'100ERR', b'OR[7]: MD5 failure']), (1, 2))

    def test_device_error(self):
        self.assertEqual(self.transfer([b'100', b'ERROR[7]: MD5 failure']), (1, 2))


if __name__ == '__main__':
    unittest.main()
