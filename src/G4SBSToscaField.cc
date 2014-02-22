#include "G4SBSToscaField.hh"

#define MAXBUFF 1024

G4SBSToscaField::G4SBSToscaField( const char *filename) 
    : G4SBSMappedField( offset, rm,  filename ) 
{
	ReadField();    
}


G4SBSToscaField::~G4SBSToscaField() {
    int i,j,k;

    for( i = 0; i < fN[0]; i++ ){
	for( j = 0; j < fN[1]; j++ ){
	    for( k = 0; k < fN[2]; k++ ){
		delete fFieldVal[i][j][k];
	    }
	    delete fFieldVal[i][j];
	}
	delete fFieldVal[i];
    }
    delete fFieldVal;

    fFieldVal = NULL;

    delete frm;
    return;
}

void G4SBSToscaField::GetFieldValue(const double Point[3],double *Bfield) const {
    double s[3];
    int idx, jdx;
    int i, j, k;
    double sx, sy, sz;

    double point[3];

    G4ThreeVector pt(Point[0], Point[1], Point[2]);
    pt = ((*frm)*pt) - fOffset;

//    printf("Querying point %f %f %f\n", pt[0]/m, pt[1]/m, pt[2]/m);

    // Use a trilinear interpolation
    // Convert fed point to field map coordinates (180 degree rotation about y)
    // Overall - is from wrong polarity in field, primarily goes down +x-axis
    // which would deflect protons into the floor
    point[0] =  pt[0];
    point[1] = -pt[1];
    point[2] =  pt[2];

    // Calculate index values for position

    for( idx = 0; idx < 3; idx++ ){
	s[idx] = (point[idx] - fMin[idx])/(fMax[idx] - fMin[idx]);


	if( s[idx] < 0.0 || s[idx] >= 1.0 ){
	    // Out of range, return 0 field
//	    printf("Out of range\n");
	    for( jdx = 0; jdx < 3; jdx++ ){
		Bfield[jdx] = 0.0;
	    }
	    return;
	}
    }

    i = (int) floor( (fN[0]-1)*s[0] );
    j = (int) floor( (fN[1]-1)*s[1] );
    k = (int) floor( (fN[2]-1)*s[2] );

    sx = (fN[0]-1)*s[0] - (double) i;
    sy = (fN[1]-1)*s[1] - (double) j;
    sz = (fN[2]-1)*s[2] - (double) k;


    // Perform interpolation
    double interp[3];
    double c00, c10, c01, c11, c0, c1;

    for( idx = 0; idx < 3; idx++ ){
	c00 = fFieldVal[i][j][k][idx]*(1.0-sx) + fFieldVal[i+1][j][k][idx]*sx;
	c10 = fFieldVal[i][j+1][k][idx]*(1.0-sx) + fFieldVal[i+1][j+1][k][idx]*sx;
	c01 = fFieldVal[i][j][k+1][idx]*(1.0-sx) + fFieldVal[i+1][j][k+1][idx]*sx;
	c11 = fFieldVal[i][j+1][k+1][idx]*(1.0-sx) + fFieldVal[i+1][j+1][k+1][idx]*sx;

	c0 = c00*(1.0-sy) + c10*sy;
	c1 = c01*(1.0-sy) + c11*sy;

	interp[idx] = c0*(1.0-sz) + c1*sz;
    }

    ///////////////////////////////////////////////
    // Make sure to put in local coordinates
    Bfield[0] = -interp[0];
    Bfield[1] =  interp[1];
    Bfield[2] = -interp[2];


    G4ThreeVector newB(Bfield[0], Bfield[1], Bfield[2]);

//    printf("%f %f %f -> %f %f %f\n", point[0]/cm, point[1]/cm, point[2]/cm, Bfield[0]/tesla, Bfield[1]/tesla, Bfield[2]/tesla );

    // Rotate to global coordinates
    newB = (frm->inverse())*newB;

    if( !fInverted ){
	Bfield[0] = newB.x();
	Bfield[1] = newB.y();
	Bfield[2] = newB.z();
    } else {
	//  Flip sign of field if geantino
	//  since it's backwards and stupid
	Bfield[0] = -newB.x();
	Bfield[1] = -newB.y();
	Bfield[2] = -newB.z();
    }
    

    //printf("Returning %f %f %f\n", Bfield[0], Bfield[1], Bfield[2]);

    return;
}

void G4SBSToscaField::ReadField(){
    printf("G4SBSToscaField - Reading in field from %s\n", fFilename);
    FILE *f = fopen(fFilename, "r");

    if( !f ){
	fprintf(stderr, "Error: %s Line %d, %s - File %s could not be opened\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, fFilename); 
	exit(1);
    }

    double x,y,z, ang;

    // First line is the position offset
    fscanf(f, "%lf%lf%lf", &x, &y, &z);
    fOffset = G4ThreeVector(x,y,z);
    
    // Second line is the rotation
    
    fscanf(f, "%lf", &ang );
    frm = new G4RotationMatrix();
    frm->RotateY(ang*deg);

    // Third line should have 4 values with the size of the
    // file indices
    
    int dint, idx, i,j,k;
    double x[3], fB[3];

    char dstring[MAXBUFF];

    fscanf(f, "%d%d%d%d", &fN[2], &fN[1], &fN[0], &dint);

    /*

    // Ensure we have enough space to read this
    if( fN[0] > MAXPT || fN[1] > MAXPT || fN[2] > MAXPT  ){
	fprintf(stderr, "Error: %s Line %d, %s - File %s is too big\nRead parameters nx = %d ny = %d nz = %d, but MAXPT = %d\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, fFilename, fN[0], fN[1], fN[2], MAXPT ); 
	exit(1);
    }
    */

    // Dynamically allocate table
    fFieldVal = new double *** [fN[0]];
    for( i = 0; i < fN[0]; i++ ){
	fFieldVal[i] = new double ** [fN[1]];
	for( j = 0; j < fN[1]; j++ ){
	    fFieldVal[i][j] = new double * [fN[2]];
	    for( k = 0; k < fN[2]; k++ ){
		fFieldVal[i][j][k] = new double[3];
	    }
	}
    }

    dstring[0] = 'x';

    while( dstring[0] != '0' ){
	if( !fgets(dstring, MAXBUFF, f) ){
	    fprintf(stderr, "Error: %s Line %d, %s - File %s has line too long (> %d)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, fFilename, MAXBUFF); 
	    exit(1);
	}
    }

    for( idx = 0; idx < 3; idx++ ){
	fMin[idx] =  1e9;
	fMax[idx] = -1e9;
    }

    // Read in the field map

    for( i = 0; i < fN[0]; i++ ){
	for( j = 0; j < fN[1]; j++ ){
	    for( k = 0; k < fN[2]; k++ ){
		fscanf(f, "%lf%lf%lf%lf%lf%lf", &x[0], &x[1], &x[2], &fB[0], &fB[1], &fB[2]);

//		printf("%f %f %f %f %f %f\n", x[0], x[1], x[2], fB[0], fB[1], fB[2]);

		// Grab limits as we go along. 
		for( idx = 0; idx < 3; idx++ ){
		    if( x[idx]*cm < fMin[idx] ){ 
			fMin[idx] = x[idx]*cm; 
		    }
		    if( x[idx]*cm > fMax[idx] ){ 
			fMax[idx] = x[idx]*cm;
		    }

		    fFieldVal[i][j][k][idx] = fB[idx]*gauss;
		}

	    }
	}
    }

    printf("G4SBSToscaField - Field complete\n");

    return;
}
