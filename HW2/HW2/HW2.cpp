#include <stdlib.h>
#include <gluit.h>
#include "gl/glpng.h"
#include "svl/svl.h"

#pragma comment(lib,"svl-vc9.lib")
#pragma comment (lib, "glpngd-vc9.lib")
#pragma comment (lib, "gluit-vc9.lib")

#define MAX 100.
#define MIN -100.
#define GRIDPTS 30       // THE SIZE OF PNG
#define INTERVALS (GRIDPTS-1)

pngRawInfo info;

float y[GRIDPTS][GRIDPTS];
int gw = 800, gh = 800;

float angle;
float _distance;
float speed = 0.0;
float maxspeed = 30.0, minspeed = -30.0;
float x, z;			// current pos
float current_origin_x, current_origin_z;

Vec3 move = Vec3(0,0,0);
Vec3 tri[3];

GLuint texid;  
GLuint pmode = GL_LINE;

GLMmodel *car;

float bilinear(float x, float z)
{
	if(x >= MAX || x <= MIN || z >= MAX || z <= MIN)
		return 0;

	float spacing = (MAX - MIN) / INTERVALS;
	int i = (x - MIN) / spacing;
	int j = (z - MIN) / spacing;
	
	float s = (x - (MIN + spacing * i)) / spacing;
	float t = (z - (MIN + spacing * j)) / spacing;

	return (1 - t) * (1 - s) * y[i][j] + (1 - t) * s * y[i + 1][j] + t * (1 - s) * y[i][j + 1] + s * t * y[i + 1][j + 1];
}

void setheight(void)
{
	for(int i = 0; i < GRIDPTS; i++) 
		for(int j = 0; j < GRIDPTS; j++) 
			y[j][i] = 20.0 * (255 - info.Data[i * info.Width + j]) / 255;  // [0.0, 20.0]
}

void init(void)
{
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.6, 0.6, 0.6, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	pngLoadRaw("terrain5.png", &info);
	setheight();
	move[1] = bilinear(move[0], move[2]);
	tri[0] = Vec3(0, bilinear(0, 0.5), 0.5);
	tri[1] = Vec3(2.5, bilinear(2.5, 0), 0);
	tri[2] = Vec3(0, bilinear(0, -0.5), -0.5);

	car = glmReadOBJ("porsche.obj");
	glmUnitize(car);
	glmScale(car, 15);
	glmFacetNormals(car);
	glmVertexNormals(car, 90);

	glEnable(GL_TEXTURE_2D);
}

void inittex(void)
{
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	texid = pngBind("terrain5.png", PNG_NOMIPMAP, PNG_SOLID, NULL, GL_CLAMP, GL_LINEAR, GL_LINEAR);

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

	float splanes[] = {1.0 / (MAX - MIN), 0, 0, -MIN / (MAX - MIN)};
	float tplanes[] = {0, 0, 1.0 / (MAX - MIN), -MIN / (MAX - MIN)};
	
	glTexGenfv(GL_S, GL_OBJECT_PLANE, splanes);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tplanes);
}

void terrain(void)
{
    float spacing = (MAX - MIN) / INTERVALS;

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPolygonMode(GL_FRONT_AND_BACK, pmode);
	
	// terrain dimension
	// [MIN, MAX] x [0, 20] x [MIN, MAX]
	glBegin(GL_QUADS);
	for(int i = 0; i < INTERVALS; i++)
		for(int j = 0; j < INTERVALS; j++){
			glVertex3f(MIN + i * spacing, y[i][j], MIN + j * spacing);
			glVertex3f(MIN + (i + 1) * spacing, y[i+1][j], MIN + j * spacing);
			glVertex3f(MIN + (i + 1) * spacing, y[i+1][j+1], MIN + (j + 1) * spacing);
			glVertex3f(MIN + i * spacing, y[i][j+1], MIN + (j + 1) * spacing);
		}
	glEnd();
	glPopAttrib();
}

void display(void)
{
#if 1
	glViewport(0, 0, gw, gh / 5 * 4);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(80, 1, 1, 400);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	x = current_origin_x + _distance * cos(angle * vl_pi / 180);
	z = current_origin_z + _distance * sin(angle * vl_pi / 180);

	float y = bilinear(x, z);
	Vec3 tmove, ttri[3];

	tmove = proj(HTrans4(Vec3(x, y, z)) * HRot4(Vec3(0, -1, 0), angle / 180 * vl_pi) * Vec4(move, 1));
	for(int i = 0; i < 3; i++){
		ttri[i] = proj(HTrans4(Vec3(x, 0, z)) * HRot4(Vec3(0, -1, 0), angle / 180 * vl_pi) * Vec4(tri[i], 1));
		ttri[i][1] = bilinear(ttri[i][0], ttri[i][2]);
	}

	// car configuration
	Vec3 xx, yy, zz, tmp;

	tmp = ttri[1] - (ttri[0] + ttri[2]) / 2;
	tmp[1] = 0;
	yy = norm(cross((ttri[0] - ttri[2]), (ttri[1] - ttri[2])));
	xx = norm(tmp - dot(yy, tmp) * yy);
	zz = cross(xx, yy);

	float mat[16] = {xx[0], xx[1], xx[2], 0.0, 
					 yy[0], yy[1], yy[2], 0.0, 
					 zz[0], zz[1], zz[2], 0.0,
					 x, y + 4.5, z, 1.0};

	// testing
	/*
	cout << "ttri[0] = " << ttri[0] << endl << "ttri[1] = " << ttri[1] << endl << "ttri[2] = " << ttri[2] << endl;
	cout << "ttri[0] - ttri[2] = " << ttri[0] - ttri[2] << endl << "ttri[1] - ttri[2] = " << ttri[1] - ttri[2] << endl;
	cout << "cross((ttri[0] - ttri[2]), (ttri[1] - ttri[2])) = " << cross((ttri[0] - ttri[2]), (ttri[1] - ttri[2])) << endl;
	cout << "yy = norm(cross((ttri[0] - ttri[2]), (ttri[1] - ttri[2])))\n = " << norm(cross((ttri[0] - ttri[2]), (ttri[1] - ttri[2]))) << endl << endl;

	printf("(%f, %f, %f)\nxx = (%f, %f, %f)\nyy = (%f, %f, %f)\nzz = (%f, %f, %f)\n", x, y, z, xx[0], xx[1], xx[2], yy[0], yy[1], yy[2], zz[0], zz[1], zz[2]);
	printf("ttri = (%f, %f, %f)\n", ttri[0][0], ttri[0][1], ttri[0][2]);
	printf("tmp = (%f, %f, %f)\n", tmp[0], tmp[1], tmp[2]);
	system("CLS");
	*/
	
	gluLookAt(0, 100, 150, 0, 0, 0, 0, 1, 0);

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_LIGHTING);

	glColor3f (1,1,1);
	terrain();

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glPushMatrix();
	glMultMatrixf(mat);
	glRotatef(-90, 0, -1, 0);
	glmDraw(car, GLM_MATERIAL | GLM_SMOOTH);
	glPopMatrix();
	glDisable(GL_LIGHTING);
#endif
#if 1
	// map
	glViewport(gw / 5 * 4 + 1, gh / 5 * 4 + 1, gw / 5, gh / 5);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(80, 1, 1, 400);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gluLookAt(0, 150, 1, 0, 0, 0, 0, 1, 0);

	glColor3f (1,1,1);
	terrain();
	
	glPushAttrib(GL_ENABLE_BIT);
	glColor3f(1, 0, 0);
	glPointSize(7);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_POINT_SMOOTH);
	glBegin(GL_POINTS);
	glVertex3dv(tmove.Ref());
	glEnd();
	glPopAttrib();
#endif

	glutSwapBuffers();
}

void reshape(int w, int h)
{
	gw = w;
	gh = h;
}

void idle(void)
{
	static int last;
	int now;
	float dt;

	if(last == 0){
		last = glutGet(GLUT_ELAPSED_TIME);
		return;
	}
	now = glutGet(GLUT_ELAPSED_TIME);
	dt = (now - last) / 1000.0;
	last = now;

	_distance += speed * dt;
	glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y)
{
	switch(key){
	case 'l': 
	case 'L':
		pmode = GL_LINE;
		break;
	case 't': 
	case 'T':
		pmode = GL_FILL;
		break;
	}
	glutPostRedisplay();
}


void special(int key, int mx, int my)
{
	switch(key){
	case GLUT_KEY_UP:
		if (speed < maxspeed)
			speed += 0.5;
		break;
	case GLUT_KEY_DOWN:
		if (speed > minspeed)
			speed -= 0.5;
		break;
	case GLUT_KEY_RIGHT:
		current_origin_x = x; 
		current_origin_z = z;
		_distance = 0.0;
		angle += 2;
		break;
	case GLUT_KEY_LEFT:
		current_origin_x = x; 
		current_origin_z = z;
		_distance = 0.0;
		angle -= 2;
		break;
	}
	glutPostRedisplay();
}

void main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitWindowSize(800, 800);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);

	glutCreateWindow ("HW2 [L | T | arrow keys]");

	glutDisplayFunc(display);
	glutReshapeFunc(reshape);

	glutKeyboardFunc(keyboard);

	init();
	inittex();

	glutSpecialFunc(special);

	glutIdleFunc(idle);

	glutMainLoop();
}
