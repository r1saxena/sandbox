# -*- coding: utf-8 -*-
"""
m-lateration solver & graph iterator
simple implementation of m-lateration position estimation 
given distance and reference point.
graph solver : given a distance graph and some know points,
try to solve all points position.
author : nicolas gaude
date : 2014-21-17
"""

import math
import numpy as np


class mlaterationsolver:
    def __init__(self, Xi, Di, lr = 0.01):
        '''
        create a m-lateration solver for 
        - m 'Xi' 2d-points [xi,yi]
        - m 'Di' distances di
        perfect solution is a 2d-point X [x,y] 
        where X is for all i<m at exactely Di distance from point Xi
        if no perfect solution can be found, approximation minimizing
        m 'Di to Xi' constaints with gradient descent and learning rate r
        '''
        assert len(Di) > 2
        assert len(Xi) == len(Di)
        self.m = len(Di)
        self.Xi = np.array(Xi)
        self.Xi.shape = (self.m, 2)
        Di = np.array(Di)
        Di.shape = (self.m)
        self.Di2 = np.square(Di)
        self.lr = lr
        return
    
    def __A(self, X):
        '''
        compute a m vector = [ ..., (x-xi)^2 + (y-yi)^2 - di^2, ....]
        '''
        A = np.square(self.__XXi(X)).sum(axis = 1) - self.Di2
#        A = np.divide(A,self.Di2)
        return A
    
    def cost(self, X):
        '''
        compute cost of m-lateration for X
        X vector = [x,y] is a solution of m-lateration is cost = 0
        cost = sum i<m  (xi-x)^2+(y-yi)^2 - di^2
        '''
        return math.pow(np.square(self.__A(X)).sum(),0.25)
    
    def XXi(self,X):
        return self.__XXi(X)
    
    def __XXi(self, X):
        '''
        compute a m*2  matrix of vector = [ ..., [X-Xi], ...]
        where Xi is a vector = [xi,yi]
        '''
        assert len(X) == 2
        XX = np.atleast_2d(X).repeat(self.m, axis=0)
        return XX - self.Xi
    
    def grad(self, X):
        '''
        compute gradient of m-lateration at X
        grad is a vector = __A * __XXi
        '''
        return np.dot(self.__A(X), self.__XXi(X))
    
    def iterate(self, X):
        '''
        each iteration returns:
        - an update X position vector
        - the dX move vector
        '''
        assert len(X) == 2
        grad = self.grad(X)
        norm  = np.linalg.norm(grad)
        if (norm < 1):
            dX = - self.lr * grad
        else:
            dX = - self.lr * grad/norm
        X = np.array(X) + dX
        #print X,'dX=',dX,self.cost(X)
        return (X,dX)
    
    def solve(self):
        '''
        return a 'local-optimal' solution of m-lateration
        '''
        # initialize the X candidate a the centroid of Xi's
        X = self.Xi.sum(axis=0)/(1.0*len(self.Xi))
        # stop gradient descent when convergence is back tracking
        i = 0
        while (i < 3333):
            i += 1
            (X,dX) = self.iterate(X)
        print X,' solved @iteration =',i, ' lr = ',self.lr
        return X


class mlaterationgraph:
    ''' 
    a lateration graph is an undirected distance graph
    some position vertex are known
    to solve others position,
    iterate to find position for missing vertex position using m-lateration
    '''
    def __init__(self):
        # dict of known vertex position
        self.pos = {}
        # set of vertex
        self.vertex = set()
        # adjacency list of set of edge : 
        # key = vertex initial
        # value = ( ..., (vertex final, distance), ...)
        self.edge = {}
        return
        
    def add_edge(self, u, v, d):
        '''
        add an undirected distance d edge between vertex u and v
        '''
        assert u != v
        self.vertex.add(u)
        self.vertex.add(v)
        self.edge.setdefault(u,set()).add((v,d))
        self.edge.setdefault(v,set()).add((u,d))
    
    def add_position(self, u, X):
        '''
        add a new 2d-point position information for vertex u
        '''
        assert len(X) == 2
        assert u
        assert not u in self.pos
        self.vertex.add(u)
        self.pos[u] = X
        
    
    def unsolved(self):
        return self.vertex.difference(self.pos.keys())
    
    def solvable(self):
        '''
        return the list of 
        solvable position with their associated m-lateration points
        number of known points m shall be at least 3 per solvable position
        '''
        solvable = []
        unsolved = self.vertex.difference(set(self.pos.keys()))
        
        for u in unsolved:
            l = filter(lambda v: v[0] in self.pos, self.edge[u])
            if len(l)>2:
                (V, Di) = zip(*l)
                Xi = [ self.pos[v] for v in V ]
                solvable.append((u, Xi, Di))
        return solvable
    
    def iterate(self):
        solved = []
        solvable = self.solvable()
        for (u, Xi, Di) in solvable:
            ml = mlaterationsolver(Xi,Di)
            X = ml.solve()
            self.add_position(u, X)
            solved.append(u)
        return solved
    
    def solve(self):
        i =0
        while i < 1000:
            print i,': unsolved positions =', len(self.unsolved())
            solved = self.iterate()
            i += 1
            if not solved:
                unsolvable = self.unsolved()
                print 'unsolvable position = ', len(unsolvable)
                break
        return
