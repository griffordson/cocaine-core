#!/usr/bin/env python
#
#    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
#    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.
#
#    This file is part of Cocaine.
#
#    Cocaine is free software; you can redistribute it and/or modify
#    it under the terms of the GNU Lesser General Public License as published by
#    the Free Software Foundation; either version 3 of the License, or
#    (at your option) any later version.
#
#    Cocaine is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#    GNU Lesser General Public License for more details.
#
#    You should have received a copy of the GNU Lesser General Public License
#    along with this program. If not, see <http://www.gnu.org/licenses/>. 
#

import zmq
from sys import argv
from pprint import pprint


def main(apps):
    context = zmq.Context()
    
    request = context.socket(zmq.REQ)
    request.connect('tcp://localhost:5000')

    request.send_json({
        'version': 2,
        'action': 'delete',
        'apps': apps
    })

    pprint(request.recv_json())


if __name__ == "__main__":
    if len(argv) == 1:
        print "Usage: %s <app-name-1> ... <app-name-N>" % argv[0]
    else:
        main(argv[1:])
