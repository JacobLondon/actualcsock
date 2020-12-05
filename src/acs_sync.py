"""
Server software
"""

import socketserver
import struct
import sys
import time
from typing import Dict, List

class AcsSync:
    def __init__(self, host: str, port: int, flatsize: int, max_clients: int):
        # UID: Raw flatdata as bytes
        self.clients: Dict[int, bytes] = {}
        self.uid_reuse: List[int] = []
        self.uid_current: int = 1
        self.host: str = host
        self.port: int = port
        self.flatsize: int = flatsize
        self.max_clients: int = max_clients

    ##
    # Get the next available UID
    def uid_get(self):
        if self.uid_reuse:
            rv = self.uid_reuse[0]
            self.uid_reuse.remove(self.uid_reuse[0])
            return rv

        rv = self.uid_current
        self.uid_current += 1
        return rv

    ##
    # Delete the given UID
    def uid_del(self, uid: int):
        if not uid in self.uid_reuse:
            self.uid_reuse.append(uid)

            del self.clients[uid]

    ##
    # Start the server, this function won't return
    def run(self):
        class AcsTcpHandler(socketserver.BaseRequestHandler):
            thisref = self

            def handle(self):
                this = AcsTcpHandler.thisref
                uid = 0
                tmp = 0

                # don't accept if too many clients
                if len(this.clients) >= this.max_clients:
                    return

                self.request.setblocking(True)

                while True:
                    try:
                        self.data = self.request.recv(this.flatsize)
                        #print(self.data)
                    except:
                        break

                    # disconnected
                    if len(self.data) == 0:
                        break

                    tmp = self.data[:4]
                    tmp = int.from_bytes(tmp, byteorder='little')

                    # need to assign a UID to this new user
                    if tmp == 0:
                        uid = this.uid_get()

                    #print(uid, self.data)

                    # save the client
                    this.clients[uid] = self.data

                    #print(this.clients)

                    # construct header, 2 uint32's
                    header = struct.pack("II", uid, len(this.clients) - 1)

                    try:
                        # send the header
                        self.request.sendall(header)

                        # send each client who isn't this one
                        for client_uid, data in this.clients.items():
                            if client_uid != uid:
                                self.request.sendall(data)
                    except:
                        break

                this.uid_del(uid)
            # end handle
        # end class
        with socketserver.ThreadingTCPServer((self.host, self.port), AcsTcpHandler) as server:
            server.serve_forever()

def _arg_get(args: list, da: str, ddarg: str) -> str:
    if da in args:
        idx = args.index(da)
        if idx + 1 < len(args):
            return args[idx + 1]
    elif ddarg in args:
        idx = args.index(ddarg)
        if idx + 1 < len(args):
            return args[idx + 1]
    return None

def _arg_check(args: list, da: str, ddarg: str) -> bool:
    return da in args or ddarg in args

if __name__ == '__main__':
    host = "localhost"
    port = 9999
    size = 64
    max_clients = 16

    if len(sys.argv) > 1:
        tmp = _arg_get(sys.argv, "-a", "--address")
        if tmp: host = tmp

        tmp = _arg_get(sys.argv, "-p", "--port")
        if tmp: port = tmp

        tmp = _arg_get(sys.argv, "-s", "--size")
        if tmp: size = int(tmp)

        tmp = _arg_get(sys.argv, "-c", "--connections")
        if tmp: max_clients = int(tmp)

        if _arg_check(sys.argv, "-h", "--help"):
            print(f"""\
{sys.argv[0]} [OPTIONS]

OPTIONS:
    -a; --address ADDRESS: Specify IPv4 ADDRESS to host
    -p; --port PORT:       Specify PORT to host at
    -s; --size SIZE:       Specify max buffer SIZE
    -c; --connections NUM: Specify max NUM of clients
    -h; --help:            See this help
""")
            exit(0)

    sync = AcsSync(host, port, size, max_clients)
    sync.run()
    exit(0)
