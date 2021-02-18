#include <vector>
#include <iostream>
#include <cstdio>

//OpenGL 
#include <gl\glew.h>
#include <gl\freeglut.h>

//GLM
#include <glm\vec3.hpp>
#include <glm\trigonometric.hpp>

//Assimp, model loading
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

//Timer
#include "Timer.h"
#include "RayTracing.h"

//VBO structure
struct PointData
{
	glm::vec3 v;  //vertex
	glm::vec3 n;  //normal
	glm::vec3 c;  //color 
};

//group name & triangle data
std::vector<GLuint> VBOs;
std::vector<std::pair<std::string, std::vector<PointData>>> rawdata;

#pragma region Others
//timer
static Timer timer;
static int rcount = 0;
static double tcount = 0;

//dragon
/* 
static float cx = 000, cy = 750, cz = 2150, rx = 000, ry = 750, rz = 1550;
static float lx = 500.0, ly = 1250.0, lz = -150.0;
static float offset = 50.0f;
static float model_scale = 100.0f;
*/

// cornell box
static float cx = 0, cy = 100, cz = 275, rx = 0, ry = 100, rz = 195;
static float lx = 0, ly = 180, lz = 70;

static float offset = 10.0f;
static float model_scale = 100.0f;

#pragma endregion

void assimpReadModelrecursive(std::vector< std::pair< std::string, std::vector<PointData> > > &data, const aiScene *scene, const aiNode* node)
{
	
	const aiFace* face;
	const aiVector3D *v1, *v2, *v3, *n1, *n2, *n3;
	const aiMaterial* material;
	const aiMesh* mesh;
	PointData temp;

	aiColor4D ka, ks, kd;
	glm::dvec3 kaa, kss, kdd;

	if (node->mNumMeshes != 0)
	{
		data.emplace_back(node->mName.C_Str(), std::vector<PointData>());
		auto& parse = data.back().second;

		for (int i = 0; i < node->mNumMeshes; i++)
		{
			mesh = scene->mMeshes[node->mMeshes[i]];
			for (int j = 0; j < mesh->mNumFaces; j++)
			{
				face = &(mesh->mFaces[j]);
				if (face->mNumIndices != 3) continue;
				v1 = &mesh->mVertices[face->mIndices[0]];
				v2 = &mesh->mVertices[face->mIndices[1]];
				v3 = &mesh->mVertices[face->mIndices[2]];
				n1 = &mesh->mNormals[face->mIndices[0]];
				n2 = &mesh->mNormals[face->mIndices[1]];
				n3 = &mesh->mNormals[face->mIndices[2]];

				material = scene->mMaterials[mesh->mMaterialIndex];

				if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &kd)) kdd = glm::vec3(kd.r, kd.g, kd.b);
				else kdd = glm::vec3(1, 1, 1);

				//push data
				
				temp.v = glm::vec3(v1->x, v1->y, v1->z) * model_scale;
				temp.n = glm::vec3(n1->x, n1->y, n1->z);
				temp.c = kdd;
				parse.push_back(temp);

				temp.v = glm::vec3(v2->x, v2->y, v2->z) * model_scale;
				temp.n = glm::vec3(n1->x, n1->y, n1->z);
				temp.c = kdd;
				parse.push_back(temp);

				temp.v = glm::vec3(v3->x, v3->y, v3->z) * model_scale;
				temp.n = glm::vec3(n1->x, n1->y, n1->z);
				temp.c = kdd;
				parse.push_back(temp);
			}
		}
	}

	for (int i = 0; i < node->mNumChildren; i++)
	{
		assimpReadModelrecursive(data, scene, node->mChildren[i]);
	}
}

void setLight()
{

	//GLfloat LightAmbient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
	GLfloat LightDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	//glLightfv(GL_LIGHT1, GL_AMBIENT, LightAmbient);	// Setup The Ambient Light
	glLightfv(GL_LIGHT1, GL_DIFFUSE, LightDiffuse);		// Setup The Diffuse Light
	GLfloat LightPosition[] = { lx, ly, lz, 1.0f };
	glLightfv(GL_LIGHT1, GL_POSITION, LightPosition);	// Position The Light
}

void DISPLAY()
{
	timer.start();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	setLight();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(cx, cy, cz, rx, ry, rz, 0, 1, 0);

	glEnable(GL_COLOR_MATERIAL);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	for (int i = 0; i < VBOs.size(); i++)
	{
		glBindBuffer(GL_ARRAY_BUFFER, VBOs[i]);
		glVertexPointer(3, GL_FLOAT, sizeof(PointData), (void*)offsetof(PointData, v));
		glNormalPointer(GL_FLOAT, sizeof(PointData), (void*)offsetof(PointData, n));
		glColorPointer(3, GL_FLOAT, sizeof(PointData), (void*)offsetof(PointData, c));
		glDrawArrays(GL_TRIANGLES, 0, rawdata[i].second.size());
	}

#pragma region EXTENSION: acceleration structure
#ifdef RAYTRACING
	rtBuildKDtreeCurrentSceneEXT();
#endif
#pragma endregion

	glFlush();
	glutSwapBuffers();

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisable(GL_COLOR_MATERIAL);

#pragma region Others
	//timer
	timer.stop();
	tcount += timer.getElapsedTime();
	rcount++;
	if (tcount > 1.0)
	{
		auto fps = std::to_string(rcount / tcount);
		fps.erase(fps.begin() + fps.find_first_of('.') + 3, fps.end());
		glutSetWindowTitle(fps.append(" frame per sec").data());
		tcount -= 1.0;
		rcount = 0;
	}
#pragma endregion

}

void Idle()
{
	glutPostRedisplay();
}

void KeyBoard(unsigned char key, int x, int y)
{
	if (key == 'z')
	{
		printf( "cx = %f, cy = %f, cz = %f\n"
				"rx = %f, ry = %f, rz = %f\n"
				"lx = %f, ly = %f, lz = %f\n",cx,cy,cz,rx,ry,rz,lx,ly,lz );
	}

	//rotate camera
	if (key == 'w') cz -= offset, rz -= offset;
	if (key == 's') cz += offset, rz += offset;
	if (key == 'a') cx -= offset, rx -= offset;
	if (key == 'd') cx += offset, rx += offset;
	if (key == 'q') rx -= offset;
	if (key == 'e') rx += offset;
	if (key == 'x') cy += offset, ry += offset;
	if (key == 'c') cy -= offset, ry -= offset;


	//switch & move light 
	if (key == 'l') glEnable(GL_LIGHTING);
	if (key == ';') glDisable(GL_LIGHTING);
	if (key == 't') ly += offset;
	if (key == 'g') ly -= offset; 
	if (key == 'h') lx += offset; 
	if (key == 'f') lx -= offset; 
	if (key == 'y') lz += offset; 
	if (key == 'r') lz -= offset; 
}

int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(600, 100);
	glutCreateWindow("");
	glutIdleFunc(Idle);
	glutDisplayFunc(DISPLAY);
	glutKeyboardFunc(KeyBoard);

	// Must be done after glut is initialized!
	GLenum res = glewInit();
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}

#pragma region Ray Tracing INIT
#ifdef RAYTRACING
	//RT library init
	rtInit();
#endif
#pragma endregion

	//load model
	const aiScene *scene = aiImportFile(argv[1], aiProcess_Triangulate | aiProcess_GenNormals |aiProcess_ImproveCacheLocality);
	if (!scene){
		printf("Can't read model\n");
		system("pause");
		return 0;
	}
	assimpReadModelrecursive(rawdata, scene, scene->mRootNode);
	aiReleaseImport(scene);

	VBOs.resize(rawdata.size());
	glGenBuffers(rawdata.size(), VBOs.data());

	for (int i = 0; i < VBOs.size(); i++)
	{
		const auto& vboname = rawdata[i].first;
		const auto& vbodata = rawdata[i].second;
		glBindBuffer(GL_ARRAY_BUFFER, VBOs[i]);
		if (vboname.find("leftWall") != std::string::npos) rtMaterialEXT(RT_MAT_MIRROR);
		//if (vboname.find("tallBox") != std::string::npos) rtMaterialEXT(RT_MAT_DIELECTRIC, 1.33f);
		//if (vboname.find("Cube") != std::string::npos) rtMaterialEXT(RT_MAT_DIELECTRIC, 1.33f);

		glBufferData(GL_ARRAY_BUFFER, sizeof(PointData) * vbodata.size(), vbodata.data(), GL_STATIC_DRAW);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glEnable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, 4.0 / 3.0, 1, 3000.0);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT1);

	glutMainLoop();
	system("pause");
	return 0;
}

