"""
Server software
"""

import socketserver
import struct
import time
from typing import Dict, List

class AcsSync:
    def __init__(self, host: str, port: int, flatsize: int):
        # UID: Raw flatdata as bytes
        self.clients: Dict[int, bytes] = {}
        self.uid_reuse: List[int] = []
        self.uid_current: int = 1
        self.host: str = host
        self.port: int = port
        self.flatsize: int = flatsize

    def uid_get(self):
        if self.uid_reuse:
            rv = self.uid_reuse[0]
            self.uid_reuse.remove(self.uid_reuse[0])
            return rv

        rv = self.uid_current
        self.uid_current += 1
        return rv

    def uid_del(self, uid: int):
        if not uid in self.uid_reuse:
            self.uid_reuse.append(uid)

            del self.clients[uid]

    def run(self):
        class AcsTcpHandler(socketserver.BaseRequestHandler):
            thisref = self

            def handle(self):
                this = AcsTcpHandler.thisref
                uid = 0
                tmp = 0

                self.request.setblocking(True)

                while True:
                    try:
                        self.data = self.request.recv(this.flatsize)
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

                    # construct header
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

if __name__ == '__main__':
    sync = AcsSync("localhost", 9999, 256)
    sync.run()
