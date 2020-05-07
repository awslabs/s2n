import pytest
import threading

from common import ProviderOptions, Ciphersuites, Curves, Protocols


class Provider(object):
    """
    A provider defines a specific provider of TLS. This could be
    S2N, OpenSSL, BoringSSL, etc.
    """

    ClientMode = "client"
    ServerMode = "server"

    def __init__(self, options: ProviderOptions):
        # If the test should wait for a specific output message before beginning,
        # put that message in ready_to_test_marker
        self.ready_to_test_marker = None

        # If the test should wait for a specific output message before sending
        # data, put that message in ready_to_send_input_marker
        self.ready_to_send_input_marker = None

        # Allows users to determine if the provider is ready to begin testing
        self._provider_ready_condition = threading.Condition()
        self._provider_ready = False

        if type(options) is not ProviderOptions:
            raise TypeError

        self.options = options
        if options.mode == Provider.ServerMode:
            self.cmd_line = self.setup_server(options)
        elif options.mode == Provider.ClientMode:
            self.cmd_line = self.setup_client(options)

    def setup_client(self, options: ProviderOptions):
        """
        Provider specific setup code goes here.
        This will probably include creating the command line based on ProviderOptions.
        """
        raise NotImplementedError

    def setup_server(self, options: ProviderOptions):
        """
        Provider specific setup code goes here.
        This will probably include creating the command line based on ProviderOptions.
        """
        raise NotImplementedError

    def get_cmd_line(self):
        return self.cmd_line

    def is_provider_ready(self):
        return self._provider_ready is True

    def set_provider_ready(self):
        with self._provider_ready_condition:
            self._provider_ready = True
            self._provider_ready_condition.notify()


class Tcpdump(Provider):
    """
    TcpDump is used by the dynamic record test. It only needs to watch
    a handful of packets before it can exit.

    This class still follows the provider setup, but all values are hardcoded
    because this isn't expected to be used outside of the dynamic record test.
    """
    def __init__(self, options: ProviderOptions):
        Provider.__init__(self, options)

    def setup_client(self, options: ProviderOptions):
        self.ready_to_test_marker = 'listening on lo'
        tcpdump_filter = f"dst port {options.port}"

        cmd_line = ["tcpdump",
            # Line buffer the output
            "-l",

            # Only read 10 packets before exiting. This is enough to find a large
            # packet, and still exit before the timeout.
            "-c", "10",

            # Watch the loopback device
            "-i", "lo",

            # Don't resolve IP addresses
            "-nn",

            # Set the buffer size to 1k
            "-B", "1024",
            tcpdump_filter]

        return cmd_line


class S2N(Provider):
    """
    The S2N provider translates flags into s2nc/s2nd command line arguments.
    """
    def __init__(self, options: ProviderOptions):
        self.ready_to_send_input_marker = None
        Provider.__init__(self, options)

    def setup_client(self, options: ProviderOptions):
        """
        Using the passed ProviderOptions, create a command line.
        """
        cmd_line = ['s2nc', '-e']

        # This is the last thing printed by s2nc before it is ready to send/receive data
        self.ready_to_send_input_marker = 'Cipher negotiated:'

        if options.use_session_ticket is False:
            cmd_line.append('-T')

        if options.insecure is True:
            cmd_line.append('--insecure')
        else:
            cmd_line.extend(['-f', options.cert])

        if options.protocol == Protocols.TLS13:
            cmd_line.append('--tls13')
            cmd_line.extend(['-c', 'default_tls13'])
        else:
            cmd_line.extend(['-c', 'default'])

        cmd_line.extend([options.host, options.port])

        # Clients are always ready to connect
        self.set_provider_ready()

        return cmd_line

    def setup_server(self, options: ProviderOptions):
        """
        Using the passed ProviderOptions, create a command line.
        """
        self.ready_to_send_input_marker = 'Cipher negotiated:'

        cmd_line = ['s2nd', '-X', '--self-service-blinding']

        if options.key is not None:
            cmd_line.extend(['--key', options.key])
        if options.cert is not None:
            cmd_line.extend(['--cert', options.cert])

        if options.insecure is True:
            cmd_line.append('--insecure')

        # For the current s2nc code the following flag must be used for x25519
        if options.curve == Curves.X25519:
            cmd_line.extend(['-u', '20200310'])

        if options.protocol == Protocols.TLS13:
            cmd_line.append('--tls13')
            cmd_line.extend(['-c', 'default_tls13'])
        else:
            cmd_line.extend(['-c', 'default'])

        cmd_line.extend([options.host, options.port])

        return cmd_line


class OpenSSL(Provider):
    def __init__(self, options: ProviderOptions):
        self.ready_to_send_input_marker = None
        Provider.__init__(self, options)

    def setup_client(self, options: ProviderOptions):
        # s_client prints this message before it is ready to send/receive data
        self.ready_to_send_input_marker = 'Verify return code'

        cmd_line = ['openssl', 's_client']
        cmd_line.extend(['-connect', '{}:{}'.format(options.host, options.port)])

        # Additional debugging that will be captured incase of failure
        cmd_line.extend(['-debug', '-tlsextdebug'])

        if options.cert is not None:
            cmd_line.extend(['-cert', options.cert])
        if options.key is not None:
            cmd_line.extend(['-key', options.key])

        # Unlike s2n, OpenSSL allows us to be much more specific about which TLS
        # protocol to use.
        if options.protocol == Protocols.TLS13:
            cmd_line.append('-tls1_3')
        elif options.protocol == Protocols.TLS12:
            cmd_line.append('-tls1_2')
        elif options.protocol == Protocols.TLS11:
            cmd_line.append('-tls1_1')
        elif options.protocol == Protocols.TLS10:
            cmd_line.append('-tls1')

        if options.cipher is not None:
            if options.cipher == Ciphersuites.TLS_CHACHA20_POLY1305_SHA256:
                cmd_line.extend(['-ciphersuites', 'TLS_CHACHA20_POLY1305_SHA256'])
            elif options.cipher == Ciphersuites.TLS_AES_128_GCM_256:
                cmd_line.extend(['-ciphersuites', 'TLS_AES_128_GCM_SHA256'])
            elif options.cipher == Ciphersuites.TLS_AES_256_GCM_384:
                cmd_line.extend(['-ciphersuites', 'TLS_AES_256_GCM_SHA384'])

        if options.curve is not None:
            cmd_line.extend(['-curves', options.curve])

        # Clients are always ready to connect
        self.set_provider_ready()

        return cmd_line

    def setup_server(self, options: ProviderOptions):
        # s_server prints this message before it is ready to send/receive data
        self.ready_to_test_marker = 'ACCEPT'

        cmd_line = ['openssl', 's_server']
        cmd_line.extend(['-accept', '{}:{}'.format(options.host, options.port)])

        # Exit after the first connection
        cmd_line.extend(['-naccept', '1'])

        # Additional debugging that will be captured incase of failure
        cmd_line.extend(['-debug', '-tlsextdebug'])

        if options.cert is not None:
            cmd_line.extend(['-cert', options.cert])
        if options.key is not None:
            cmd_line.extend(['-key', options.key])

        # Unlike s2n, OpenSSL allows us to be much more specific about which TLS
        # protocol to use.
        if options.protocol == Protocols.TLS13:
            cmd_line.append('-tls1_3')
        elif options.protocol == Protocols.TLS12:
            cmd_line.append('-tls1_2')
        elif options.protocol == Protocols.TLS11:
            cmd_line.append('-tls1_1')
        elif options.protocol == Protocols.TLS10:
            cmd_line.append('-tls1')

        if options.cipher is not None:
            if options.cipher == Ciphersuites.TLS_CHACHA20_POLY1305_SHA256:
                cmd_line.extend(['-ciphersuites', 'TLS_CHACHA20_POLY1305_SHA256'])
            elif options.cipher == Ciphersuites.TLS_AES_128_GCM_256:
                cmd_line.extend(['-ciphersuites', 'TLS_AES_128_GCM_SHA256'])
            elif options.cipher == Ciphersuites.TLS_AES_256_GCM_384:
                cmd_line.extend(['-ciphersuites', 'TLS_AES_256_GCM_SHA384'])

        if options.curve is not None:
            cmd_line.extend(['-curves', options.curve])

        return cmd_line


class BoringSSL(Provider):
    """
    NOTE: In order to focus on the general use of this framework, BoringSSL
    is not yet supported. The client works, the server has not yet been
    implemented, neither are in the default configuration.
    """
    def __init__(self, options: ProviderOptions):
        self.ready_to_send_input_marker = None
        Provider.__init__(self, options)

    def setup_server(self, options: ProviderOptions):
        pytest.skip('BoringSSL does not support server mode at this time')

    def setup_client(self, options: ProviderOptions):
        self.ready_to_send_input_marker = 'Cert issuer:'
        cmd_line = ['bssl', 's_client']
        cmd_line.extend(['-connect', '{}:{}'.format(options.host, options.port)])
        if options.cert is not None:
            cmd_line.extend(['-cert', options.cert])
        if options.key is not None:
            cmd_line.extend(['-key', options.key])
        if options.cipher is not None:
            if options.cipher == Ciphersuites.TLS_CHACHA20_POLY1305_SHA256:
                cmd_line.extend(['-cipher', 'TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256'])
            elif options.cipher == Ciphersuites.TLS_AES_128_GCM_256:
                pytest.skip('BoringSSL does not support Cipher {}'.format(options.cipher))
            elif options.cipher == Ciphersuites.TLS_AES_256_GCM_384:
                pytest.skip('BoringSSL does not support Cipher {}'.format(options.cipher))
        if options.curve is not None:
            if options.curve == Curves.P256:
                cmd_line.extend(['-curves', 'P-256'])
            elif options.curve == Curves.P384:
                cmd_line.extend(['-curves', 'P-384'])
            elif options.curve == Curves.X25519:
                pytest.skip('BoringSSL does not support curve {}'.format(options.curve))

        # Clients are always ready to connect
        self.set_provider_ready()

        return cmd_line


