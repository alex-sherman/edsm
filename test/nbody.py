#!/usr/bin/python

import jrpc
import pygame
import random
import math
import sys

random.seed(0)

server = None

server = jrpc.service.SocketProxy(8764, host = sys.argv[1], timeout = 5) #The server's listening port

pygame.init()
screen_size = 700
screen = pygame.display.set_mode((screen_size, screen_size))
#points = [[0,0,1], [0,1,1], [1,0,1], [1,1,1]]
points = []
point_count = 30
cs = math.cos(math.pi/3)
sn = math.sin(math.pi/3)
for x in range(point_count):
    for y in range(point_count):
        dx = point_count / 2 - x
        dy = point_count / 2 - y
        d_len = math.pow(math.pow(dx,2) + math.pow(dy,2),.5)
        if d_len == 0:
            m = 1000000
            vx = 0
            vy = 0
        else:
            dx /= d_len
            dy /= d_len
            m = 1000 * random.random()
            vx = (dx * cs - dy * sn) / 100 + (random.random() - .5) / 700
            vy = (dx * sn + dy * cs) / 100 + (random.random() - .5) / 700

        points.append([1.0 * screen_size * x / point_count, 1.0 * screen_size * y / point_count,
            vx, vy,
            m])
body_count = len(points)


bid, = server.init_simulation(points, 1000, 2, 1)
print bid

while True:
    screen.fill((0,0,0))
    for point in points:
        if any([math.isnan(r) for r in point]):
            print point
        pygame.draw.circle(screen, (128,128,128), (int(point[0]), int(point[1])), int(math.pow(point[-1]/200, .5)), 0)

    pygame.display.update()
    points = server.run_simulation(body_count, bid)
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            exit()
