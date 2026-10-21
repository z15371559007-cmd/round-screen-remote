import unittest

from round_bridge.protocol import ProtocolError, authenticate, parse_command


class ProtocolTests(unittest.TestCase):
    def test_authentication_requires_hello_and_matching_token(self):
        self.assertTrue(authenticate({"type": "hello", "token": "secret"}, "secret"))
        self.assertFalse(authenticate({"type": "hello", "token": "wrong"}, "secret"))
        self.assertFalse(authenticate({"type": "command", "token": "secret"}, "secret"))
        self.assertFalse(authenticate({"type": "hello", "token": "secret"}, ""))

    def test_simple_command(self):
        command = parse_command({"type": "command", "id": "a1", "command": "play_pause"})
        self.assertEqual(command.name, "play_pause")
        self.assertIsNone(command.value)

    def test_numeric_command_validation(self):
        command = parse_command({"type": "command", "id": "a2", "command": "set_volume", "value": 0.4})
        self.assertEqual(command.value, 0.4)
        with self.assertRaises(ProtocolError):
            parse_command({"type": "command", "id": "a3", "command": "set_volume", "value": 2})
        with self.assertRaises(ProtocolError):
            parse_command({"type": "command", "id": "a4", "command": "seek_to", "value": True})

    def test_arbitrary_shell_commands_are_not_supported(self):
        with self.assertRaises(ProtocolError):
            parse_command({"type": "command", "id": "a5", "command": "shell", "value": "reboot"})


if __name__ == "__main__":
    unittest.main()
