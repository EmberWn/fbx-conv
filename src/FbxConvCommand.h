/*******************************************************************************
 * Copyright 2011 See AUTHORS file.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/
/** @author Xoppa */
#ifndef FBXCONVCOMMAND_H
#define FBXCONVCOMMAND_H

//#define ALLOW_INPUT_TYPE

#include "Settings.h"
#include <string>
#include "log/log.h"
#include <fstream>

namespace fbxconv {

struct FbxConvCommand {
	const int argc;
	const char **argv;
	int error;
	bool help;
	Settings *settings;
	log::Log *log;

	FbxConvCommand(log::Log *log, const int &argc, const char** argv, Settings *settings)
		: log(log), argc(argc), argv(argv), settings(settings), error(log::iNoError) {
		help = (argc <= 1);

		settings->flipV = false;
		settings->packColors = false;
		settings->verbose = false;
		settings->maxNodePartBonesCount = 12;
		settings->maxVertexBonesCount = 4;
		settings->maxVertexCount = (1<<15)-1;
		settings->maxIndexCount = (1<<15)-1;
		settings->outType = FILETYPE_AUTO;
		settings->inType = FILETYPE_AUTO;

		for (int i = 1; i < argc; i++) {
			const char *arg = argv[i];
			const int len = (int)strlen(arg);
			if (len > 1 && arg[0] == '-') {
				if (arg[1] == '?')
					help = true;
				else if (arg[1] == 'f')
					settings->flipV = true;
				else if (arg[1] == 'v')
					settings->verbose = true;
				else if (arg[1] == 'p')
					settings->packColors = true;
				else if ((arg[1] == 'i') && (i + 1 < argc))
					settings->inType = parseType(argv[++i]);
				else if ((arg[1] == 'o') && (i + 1 < argc))
					settings->outType = parseType(argv[++i]);
				else if ((arg[1] == 'b') && (i + 1 < argc))
					settings->maxNodePartBonesCount = atoi(argv[++i]);
				else if ((arg[1] == 'w') && (i + 1 < argc))
					settings->maxVertexBonesCount = atoi(argv[++i]);
				else if ((arg[1] == 'm') && (i + 1 < argc))
					settings->maxVertexCount = settings->maxIndexCount = atoi(argv[++i]);
				else
					log->error(error = log::eCommandLineUnknownOption, arg);
			}
			else if (settings->inFile.length() < 1)
				settings->inFile = arg;
			else if (settings->textureLoadDir.length() < 1)
				settings->textureLoadDir = arg;
			else if (settings->outFile.length() < 1)
				settings->outFile = arg;
			else
				log->error(error = log::eCommandLineUnknownArgument, arg);
			if (error != log::iNoError)
				break;
		}
		if (error == log::iNoError)
			validate();
	}

	void printCommand() const {
		for (int i = 1; i < argc; i++) {
			if (i > 1)
				printf(" ");
			printf(argv[i]);
		}
		printf("\n");
	}

	void printHelp() const {
		printf("Usage: fbx-conv.exe [options] <input> [<output>]\n");
		printf("\n");
		printf("Options:\n");
		printf("-?       : Display this help information.\n");
#ifdef ALLOW_INPUT_TYPE
		printf("-i <type>: Set the type of the input file to <type>\n");
#endif
		printf("-o <type>: Set the type of the output file to <type>\n");
		printf("-f       : Flip the V texture coordinates.\n");
		printf("-p       : Pack vertex colors to one float.\n");
		printf("-m <size>: The maximum amount of vertices or indices a mesh may contain (default: 32k)\n");
		printf("-b <size>: The maximum amount of bones a nodepart can contain (default: 12)\n");
		printf("-w <size>: The maximum amount of bone weights per vertex (default: 4)\n");
		printf("-v       : Verbose: print additional progress information\n");
		printf("\n");
		printf("<input>  : The filename of the file to convert.\n");
		printf("<output> : The filename of the converted file.\n");
		printf("\n");
		printf("<type>   : FBX, G3DJ (json) or G3DB (binary).\n");
	}
private:
	void validate() {
		if (settings->inFile.empty() || settings->textureLoadDir.empty()) {
			log->error(error = log::eCommandLineMissingInputFile);
			return;
		}
		
		if (!parseTexturePaths())
		{
			return;
		}


#ifdef ALLOW_INPUT_TYPE
		if (inType == FILETYPE_AUTO)
			inType = guessType(inFile, FILETYPE_IN_DEFAULT);
#else
		settings->inType = FILETYPE_IN_DEFAULT;
#endif
		if (settings->outFile.empty())
			setExtension(
				settings->outFile = settings->inFile, 
				".fbx",
				"_mh");
		else if (settings->outType == FILETYPE_AUTO)
			settings->outType = guessType(settings->outFile);
		if (settings->maxVertexBonesCount < 0 || settings->maxVertexBonesCount > 8) {
			log->error(error = log::eCommandLineInvalidVertexWeight);
			return;
		}
		if (settings->maxNodePartBonesCount < settings->maxVertexBonesCount) {
			log->error(error = log::eCommandLineInvalidBoneCount);
			return;
		}
		if (settings->maxVertexCount < 0 || settings->maxVertexCount > (1<<15)-1) {
			log->error(error = log::eCommandLineInvalidVertexCount);
			return;
		}
	}

	bool parseTexturePaths()
	{
		// 材质信息存储于fbx同名文本文件
		std::string material_info = settings->inFile;
		setExtension(material_info, ".txt");
		
		std::ifstream fin;
		char buff[1024];
		fin.open(material_info);

		if (fin.is_open())
		{
			printf("textures dir: %s\n", settings->textureLoadDir.c_str());
			
			std::string last_material_id = "";
			
			std::string mtd_name;
			bool mtd = false;
			
			while (!fin.eof())
			{
				memset(buff, 0, 1024);
				fin.getline(buff, 1204);
				std::string message = buff;
				
				int i = 0;
				for (; i < message.size(); ++i)
					if (!isdigit(message[i])) break;
				
				if (i == message.size())
				{
					mtd = false;
					last_material_id = message;
					printf("%s\n", last_material_id.c_str());
					settings->texturePaths[last_material_id] = std::map<std::string, std::string>{};
				}
				else
				{
					
					int e = (int)message.find_last_of('.');
					int pos = message.find_last_of('\\');
					std::string extern_name = message.substr(e + 1);
					if (extern_name == "mtd")
					{
						
						mtd_name = message.substr(pos + 1, e - pos - 1);
						printf("texture path store in mtd file\n");
						printf("path:%s\n", message.c_str());
						mtd = true;
						settings->texturePaths[mtd_name] = std::map<std::string, std::string>{};
						settings->texturePaths.erase(last_material_id);
						continue;
					}
					else if (extern_name != "tif")
					{
						printf("texture path format error\n");
						return false;
					}
					int s = (int)message.find_last_of('_');
					std::string tex_type = message.substr(s + 1, e - s - 1);
					std::string tex_key = mtd ? mtd_name : last_material_id;
					if (fbxconv::legal_postfix.find(tex_type) != fbxconv::legal_postfix.end()
						&& settings->texturePaths[tex_key].find(tex_type)
						== settings->texturePaths[tex_key].end())
					{
						
						message = settings->textureLoadDir + message.substr(pos, e - pos) + ".tga";
						printf("path:%s\n", message.c_str());
						settings->texturePaths[tex_key][tex_type] = message;
					}
						
				}
			}
			fin.close();
		}
		else
		{
			log->error(error = log::TexturePathsFileCannotOpen, material_info);
			return false;
		}
		return true;
	}

	int parseType(const char* arg, const int &def = -1) {
		if (stricmp(arg, "fbx")==0)
			return FILETYPE_FBX;
		else if (stricmp(arg, "g3db")==0)
			return FILETYPE_G3DB;
		else if (stricmp(arg, "g3dj")==0)
			return FILETYPE_G3DJ;
		if (def < 0)
			log->error(error = log::eCommandLineUnknownFiletype, arg);
		return def;
	}


	int guessType(const std::string &fn, const int &def = -1) {
		int o = (int)fn.find_last_of('.');
		if (o == std::string::npos)
			return def;
		std::string ext = fn.substr(++o, fn.length() - o);
		return parseType(ext.c_str(), def);
	}

	void setExtension(std::string &fn, const std::string &ext, std::string extra_info = "") const {
		int o = (int)fn.find_last_of('.');
		if (o == std::string::npos)
			fn += extra_info + "." + ext;
		else
			fn = fn.substr(0, o) + extra_info + ext;
	}

	void setExtension(std::string &fn, const int &type) const {
		switch(type) {
		case FILETYPE_FBX:	return setExtension(fn, ".fbx");
		case FILETYPE_G3DB:	return setExtension(fn, ".g3db");
		case FILETYPE_G3DJ:	return setExtension(fn, ".g3dj");
		default:			return setExtension(fn, "");
		}
	}
};

}

#endif //FBXCONVCOMMAND_H