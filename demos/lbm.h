/*
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2023 guruthree
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "jet.h"

class LBM {

    // reference: https://hackaday.io/project/25446-lattice-boltzmann-fluid-flow-in-matlab
    // thanks to @ZodiusInfuser for some tweaks & discussion

    public:

#if HORIZONTAL_DOUBLING == 2
        static const uint8_t NX = 52;
#else
        static const uint8_t NX = 46;
#endif
        static const uint8_t NY = 32;
        float maxVal;
        uint8_t BOUND[NX][NY];

    private:
        static const uint8_t MAX_BOUND = 255;

        // omega any higher and it dies?
        // 34x34, 1.85, 100
        static constexpr float OMEGA = 1.82f, DENSITY = 0.6f, TARGET_USUM = 150.0f;

        static constexpr float W1 = 1.0f / 9.0f;
        static constexpr float W2 = 1.0f / 36.0f;

        const uint8_t REFLECTED[8] = {4, 5, 6, 7, 0, 1, 2, 3};

        float F[NX][NY][9];
//        uint8_t BOUND[NX][NY];

        uint8_t ON[MAX_BOUND][2];
        uint8_t numBound;
        uint32_t ts; // time step
        float temp1, temp2, temp3;
        float BOUNCEDBACK[MAX_BOUND][8];
        float ux[NX][NY], speed[NX][NY];
        float rho, uy, uxx, uyy, uxy, uu;
        float FEQ[9];
        float Zts, Zdelta;
        uint8_t ii,jj,kk;

        int doBound(void) {
	        int nmBound = 0;
	        for (ii = 0; ii < NX; ii++) {
		        for (jj = 0; jj < NY; jj++) {
			        if (BOUND[ii][jj] == 1) {
				        nmBound++;
			        }
		        }
	        }

	        kk = 0;
	        for (ii = 0; ii < NX; ii++) {
		        for (jj = 0; jj < NY; jj++) {
			        if (BOUND[ii][jj] == 1) {
				        ON[kk][0] = ii;
				        ON[kk][1] = jj;
				        //printf("ON[%i] = [%i][%i] (%i?)\n", kk, ii, jj, ii+jj*NX);
				        kk++;
				        // uh oh if kk > MAX_BOUND
			        }
		        }
	        }
	        return nmBound;
        }


    public:
        LBM () {}

        void init() {

            for (ii = 0; ii < NX; ii++) {
                for (jj = 0; jj < NY; jj++) {
                    for (kk = 0; kk < 9; kk++) {
                        F[ii][jj][kk] = DENSITY / 9;
                    }
                }
            }

            memset(BOUND, 0, sizeof(uint8_t) * NX * NY);
            // walls
            for (ii = 0; ii < NX; ii++) {
                BOUND[ii][0] = 1;
            }
            for (ii = 0; ii < NX; ii++) {
                BOUND[ii][NY-1] = 1;
            }

            // flow forcing
            for (ii = 0; ii < NX; ii++) {
                for (jj = 0; jj < NY; jj++) {
                    F[ii][jj][0] = 0.0951;
                    F[ii][jj][1] = 0.0238;
                    F[ii][jj][2] = 0.0739;
                    F[ii][jj][3] = 0.0143;
                    F[ii][jj][4] = 0.0574;
                    F[ii][jj][5] = 0.0144;
                    F[ii][jj][6] = 0.0739;
                    F[ii][jj][7] = 0.0238;
                    F[ii][jj][8] = 0.2955;
                }
            }

    		numBound = doBound();
        }

        // place a cylinder in the flow
        void cylinder(uint8_t yoffset = 15, uint8_t xoffset = 8) {
            for (jj = yoffset; jj < yoffset+4; jj++) {
                BOUND[xoffset+1][jj] = 1;
                BOUND[xoffset+2][jj] = 1;
            }

            for (jj = yoffset+1; jj < yoffset+3; jj++) {
                BOUND[xoffset][jj] = 1;
                BOUND[xoffset+3][jj] = 1;
            }
            BOUND[xoffset+3][yoffset] = 1;
    		numBound = doBound();
        }


    	// add randomness by changing the boundary conditions sometimes
        void randomness () {
        	if ((float)rand()/(float)(RAND_MAX) > 0.995f) {
        		//printf("swapping boundary\n");
        		if (BOUND[11][15] == 1) {
        			BOUND[11][15] = 0;
        		}
        		else {
        			BOUND[11][15] = 1;
        		}
        		numBound = doBound();
        	}
        }

        void timestep (bool storespeed = false) {    	
            if (storespeed) {
                maxVal = 0;
            }
		
        	// [2:NX 1]
        	for (jj = 0; jj < NY; jj++) {
        		temp1 = F[0][jj][3];
        		temp2 = F[0][jj][4];
        		temp3 = F[0][jj][5];
        		for (ii = 0; ii < NX-1; ii++) {
        			F[ii][jj][3] = F[ii+1][jj][3];
        			F[ii][jj][4] = F[ii+1][jj][4];
        			F[ii][jj][5] = F[ii+1][jj][5];
        		}
        		F[NX-1][jj][3] = temp1;
        		F[NX-1][jj][4] = temp2;
        		F[NX-1][jj][5] = temp3;
        	}
//sleep_us(100);
        
        	// [NY 1:NY-1]
        	for (ii = 0; ii < NX; ii++) {
        		temp1 = F[ii][NY-1][3];
        		temp2 = F[ii][NY-1][2];
        		temp3 = F[ii][NY-1][1];
        		for (jj = NY-1; jj > 0; jj--) {
        			F[ii][jj][3] = F[ii][jj-1][3];
        			F[ii][jj][2] = F[ii][jj-1][2];
        			F[ii][jj][1] = F[ii][jj-1][1];
        		}
        		F[ii][0][3] = temp1;
        		F[ii][0][2] = temp2;
        		F[ii][0][1] = temp3;
        	}
//sleep_us(100);
        
        	// [NX 1:NX-1]
        	for (jj = 0; jj < NY; jj++) {
        		temp1 = F[NX-1][jj][1];
        		temp2 = F[NX-1][jj][0];
        		temp3 = F[NX-1][jj][7];
        		for (ii = NX-1; ii > 0; ii--) {
        			F[ii][jj][1] = F[ii-1][jj][1];
        			F[ii][jj][0] = F[ii-1][jj][0];
        			F[ii][jj][7] = F[ii-1][jj][7];
        		}
        		F[0][jj][1] = temp1;
        		F[0][jj][0] = temp2;
        		F[0][jj][7] = temp3;
        	}
//sleep_us(100);
        			
        	// [2:NY 1]
        	for (ii = 0; ii < NX; ii++) {
        		temp1 = F[ii][0][5];
        		temp2 = F[ii][0][6];
        		temp3 = F[ii][0][7];
        		for (jj = 0; jj < NY-1; jj++) {
        			F[ii][jj][5] = F[ii][jj+1][5];
        			F[ii][jj][6] = F[ii][jj+1][6];
        			F[ii][jj][7] = F[ii][jj+1][7];
        		}
        		F[ii][NY-1][5] = temp1;
        		F[ii][NY-1][6] = temp2;
        		F[ii][NY-1][7] = temp3;
        	}
//sleep_us(100);
        
        	for (kk = 0; kk < 8; kk++) {
        		for (ii = 0; ii < numBound; ii++) {
        			BOUNCEDBACK[ii][kk] = F[ON[ii][0]][ON[ii][1]][kk];
        		}
        	}
//sleep_us(100);
        
        	memset(ux, 0, sizeof(float)*NX*NY);
        	for (ii = 0; ii < NX; ii++) {
        		for (jj = 0; jj < NY; jj++) {
        			ux[ii][jj] += F[ii][jj][0];
        			ux[ii][jj] += F[ii][jj][1];
        			ux[ii][jj] += F[ii][jj][7];
        			ux[ii][jj] -= F[ii][jj][3];
        			ux[ii][jj] -= F[ii][jj][4];
        			ux[ii][jj] -= F[ii][jj][5];
        		}
        	}
//sleep_us(100);
        	
        	Zts = 0;
        	for (ii = 0; ii < NX; ii++) {
        		for (jj = 0; jj < NY; jj++) {
        			Zts += ux[ii][jj];
        		}
        	}
//sleep_us(100);
        
        	Zdelta = Zts - TARGET_USUM;
        	for (ii = 0; ii < NX; ii++) {
        		for (jj = 0; jj < NY; jj++) {
        			ux[ii][jj] -= Zdelta / (NX*NY);
        		}
        	}
//sleep_us(100);
        
        	for (ii = 0; ii < numBound; ii++) {
        		ux[ON[ii][0]][ON[ii][1]] = 0;
        	}
//sleep_us(100);
      
        	for (ii = 0; ii < NX; ii++) {
        		for (jj = 0; jj < NY; jj++) {
        			rho = 0;
        			uy = 0;
        			if (ux[ii][jj] != 0) {
        				for (kk = 0; kk < 9; kk++) {
        					rho += F[ii][jj][kk];
        				}
        
        				uy += F[ii][jj][1];
        				uy += F[ii][jj][2];
        				uy += F[ii][jj][3];
        				uy -= F[ii][jj][5];
        				uy -= F[ii][jj][6];
        				uy -= F[ii][jj][7];
        			}
        
        			uxx = ux[ii][jj] * ux[ii][jj];
        			uyy = uy * uy;
        			uxy = 2 * ux[ii][jj] * uy;
        			uu = uxx + uyy;
        	
        			if (storespeed) {
        				speed[ii][jj] = sqrtf(uxx + uyy);
                        if (speed[ii][jj] > maxVal) {
                            maxVal = speed[ii][jj];
                        }
        			}
        
        			FEQ[0] = W1 * (rho + 3*ux[ii][jj] + 4.5*uxx - 1.5*uu);
        			FEQ[2] = W1 * (rho + 3*uy + 4.5*uyy - 1.5*uu);
        			FEQ[4] = W1 * (rho - 3*ux[ii][jj] + 4.5*uxx - 1.5*uu);
        			FEQ[6] = W1 * (rho - 3*uy + 4.5*uyy - 1.5*uu);
        
        			FEQ[1] = W2 * (rho + 3*(ux[ii][jj] + uy) + 4.5*(uu + uxy) - 1.5*uu);
        			FEQ[3] = W2 * (rho - 3*(ux[ii][jj] - uy) + 4.5*(uu - uxy) - 1.5*uu);
        			FEQ[5] = W2 * (rho - 3*(ux[ii][jj] + uy) + 4.5*(uu + uxy) - 1.5*uu);
        			FEQ[7] = W2 * (rho + 3*(ux[ii][jj] - uy) + 4.5*(uu - uxy) - 1.5*uu);
        
        			FEQ[8] = rho;
        			for (kk = 0; kk < 8; kk++) {
        				FEQ[8] -= FEQ[kk];
        			}
        
        			for (kk = 0; kk < 9; kk++) {
        				F[ii][jj][kk] = OMEGA * FEQ[kk] + (1 - OMEGA)*F[ii][jj][kk];
        			}
        		}
//sleep_us(100);
        	}
        
        	for (kk = 0; kk < 8; kk++) {
        		for (ii = 0; ii < numBound; ii++) {
        			F[ON[ii][0]][ON[ii][1]][REFLECTED[kk]] = BOUNCEDBACK[ii][kk];
        		}
        	}
//sleep_us(100);
            ts++;
        }

        uint32_t getNumberOfTimeSteps() {
            return ts;
        }

        float getSpeed(uint8_t x, uint8_t y) {
            return speed[x][y];
        }

}; // end class

void drawlbm(LBM &lbm, int8_t *tbuf) {

    uint16_t xat = 0, yat = 0, speed;
    for (uint8_t y = 2; y < lbm.NY-2; y++) { // don't draw boundaries
        xat = XRESOLUTION/2 - (lbm.NX/2)*4/HORIZONTAL_DOUBLING-1;
        for (uint8_t x = 0; x < lbm.NX; x++) {
            if (!lbm.BOUND[x][y]) {
                speed = lbm.getSpeed(x, y) * 63.0 / lbm.maxVal;
                speed > 63 ? speed = 63 : 1;

                // simple nearest neighbour interpolation?
                setPixelRGBtwoX(tbuf, xat, yat, jet[speed][0], jet[speed][1], jet[speed][2]);
                setPixelRGBtwoX(tbuf, xat, yat+2, jet[speed][0], jet[speed][1], jet[speed][2]);
#if HORIZONTAL_DOUBLING == 1
                setPixelRGBtwoX(tbuf, xat+2, yat, jet[speed][0], jet[speed][1], jet[speed][2]);
                setPixelRGBtwoX(tbuf, xat+2, yat+2, jet[speed][0], jet[speed][1], jet[speed][2]);
#endif
            }
#if HORIZONTAL_DOUBLING == 2
            xat += 2;
#else
            xat += 4;
#endif
        }
        yat += 4;
    } 

}
