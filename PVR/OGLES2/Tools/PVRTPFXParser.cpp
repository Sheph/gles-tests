/******************************************************************************

 @File         PVRTPFXParser.cpp

 @Title        PVRTPFXParser

 @Version      

 @Copyright    Copyright (c) Imagination Technologies Limited.

 @Platform     Windows + Linux

 @Description  PFX file parser.

******************************************************************************/

/*****************************************************************************
** Includes
******************************************************************************/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "PVRTGlobal.h"
#include "PVRTContext.h"
#include "PVRTMatrix.h"
#include "PVRTFixedPoint.h"
#include "PVRTMisc.h"
#include "PVRTPFXParser.h"
#include "PVRTResourceFile.h"
#include "PVRTString.h"
#include "PVRTMisc.h"		// Used for POT functions

/****************************************************************************
** Constants
****************************************************************************/
const char* c_pszLinear   = "LINEAR";
const char* c_pszNearest  = "NEAREST";
const char* c_pszNone	  = "NONE";
const char* c_pszClamp    = "CLAMP";
const char* c_pszRepeat	  = "REPEAT";
const char* c_pszCurrentView = "PFX_CURRENTVIEW";

const unsigned int CPVRTPFXParser::VIEWPORT_SIZE = 0xAAAA;

const char* c_ppszFilters[eFilter_Size] = 
{ 
	c_pszNearest,		// eFilter_Nearest
	c_pszLinear,		// eFilter_Linear
	c_pszNone,			// eFilter_None
};
const char* c_ppszWraps[eWrap_Size] = 
{ 
	c_pszClamp,			// eWrap_Clamp
	c_pszRepeat			// eWrap_Repeat
};

#define NEWLINE_TOKENS "\r\n"
#define DELIM_TOKENS " \t"

/****************************************************************************
** Data tables
****************************************************************************/

/****************************************************************************
** CPVRTPFXParserReadContext Class
****************************************************************************/
class CPVRTPFXParserReadContext
{
public:
	char			**ppszEffectFile;
	int				*pnFileLineNumber;
	unsigned int	nNumLines, nMaxLines;

public:
	CPVRTPFXParserReadContext();
	~CPVRTPFXParserReadContext();
};

/*!***************************************************************************
 @Function			CPVRTPFXParserReadContext
 @Description		Initialises values.
*****************************************************************************/
CPVRTPFXParserReadContext::CPVRTPFXParserReadContext()
{
	nMaxLines = 5000;
	nNumLines = 0;
	ppszEffectFile		= new char*[nMaxLines];
	pnFileLineNumber	= new int[nMaxLines];
}

/*!***************************************************************************
 @Function			~CPVRTPFXParserReadContext
 @Description		Frees allocated memory
*****************************************************************************/
CPVRTPFXParserReadContext::~CPVRTPFXParserReadContext()
{
	// free effect file
	for(unsigned int i = 0; i < nNumLines; i++)
	{
		FREE(ppszEffectFile[i]);
	}
	delete [] ppszEffectFile;
	delete [] pnFileLineNumber;
}


/*!***************************************************************************
 @Function			IgnoreWhitespace
 @Input				pszString
 @Output			pszString
 @Description		Skips space, tab, new-line and return characters.
*****************************************************************************/
void IgnoreWhitespace(char **pszString)
{
	while(	*pszString[0] == '\t' ||
			*pszString[0] == '\n' ||
			*pszString[0] == '\r' ||
			*pszString[0] == ' ' )
	{
		(*pszString)++;
	}
}

/*!***************************************************************************
 @Function			ReadEOLToken
 @Input				pToken
 @Output			char*
 @Description		Reads next strings to the end of the line and interperts as
					a token.
*****************************************************************************/
char* ReadEOLToken(char* pToken)
{
	char* pReturn = NULL;

	char szDelim[2] = {'\n', 0};				// try newline
	pReturn = strtok(pToken, szDelim);			
	if(pReturn == NULL)
	{
		szDelim[0] = '\r';
		pReturn = strtok (pToken, szDelim);		// try linefeed
	}
	return pReturn;
}

/*!***************************************************************************
 @Function			GetSemanticDataFromString
 @Output			pDataItem
 @Modified			pszArgumentString
 @Input				eType
 @Output			pError				error message
 @Return			true if successful
 @Description		Extracts the semantic data from the string and stores it
					in the output SPVRTSemanticDefaultData parameter.
*****************************************************************************/
bool GetSemanticDataFromString(SPVRTSemanticDefaultData *pDataItem, const char * const pszArgumentString, ESemanticDefaultDataType eType, CPVRTString *pError)
{
	char *pszString = (char *)pszArgumentString;
	char *pszTmp;

	IgnoreWhitespace(&pszString);

	if(pszString[0] != '(')
	{
		*pError = CPVRTString("Missing '(' after ") + c_psSemanticDefaultDataTypeInfo[eType].pszName;
		return false;
	}
	pszString++;

	IgnoreWhitespace(&pszString);

	if(!strlen(pszString))
	{
		*pError = c_psSemanticDefaultDataTypeInfo[eType].pszName + CPVRTString(" missing arguments");
		return false;
	}

	pszTmp = pszString;
	switch(c_psSemanticDefaultDataTypeInfo[eType].eInternalType)
	{
		case eFloating:
			pDataItem->pfData[0] = (float)strtod(pszString, &pszTmp);
			break;
		case eInteger:
			pDataItem->pnData[0] = (int)strtol(pszString, &pszTmp, 10);
			break;
		case eBoolean:
			if(strncmp(pszString, "true", 4) == 0)
			{
				pDataItem->pbData[0] = true;
				pszTmp = &pszString[4];
			}
			else if(strncmp(pszString, "false", 5) == 0)
			{
				pDataItem->pbData[0] = false;
				pszTmp = &pszString[5];
			}
			break;
	}

	if(pszString == pszTmp)
	{
		size_t n = strcspn(pszString, ",\t ");
		char *pszError = (char *)malloc(n + 1);
		strcpy(pszError, "");
		strncat(pszError, pszString, n);
		*pError = CPVRTString("'") + pszError + "' unexpected for " + c_psSemanticDefaultDataTypeInfo[eType].pszName;
		FREE(pszError);
		return false;
	}
	pszString = pszTmp;

	IgnoreWhitespace(&pszString);

	for(unsigned int i = 1; i < c_psSemanticDefaultDataTypeInfo[eType].nNumberDataItems; i++)
	{
		if(!strlen(pszString))
		{
			*pError = c_psSemanticDefaultDataTypeInfo[eType].pszName + CPVRTString(" missing arguments");
			return false;
		}

		if(pszString[0] != ',')
		{
			size_t n = strcspn(pszString, ",\t ");
			char *pszError = (char *)malloc(n + 1);
			strcpy(pszError, "");
			strncat(pszError, pszString, n);
			*pError = CPVRTString("'") + pszError + "' unexpected for " + c_psSemanticDefaultDataTypeInfo[eType].pszName;
			FREE(pszError);
			return false;
		}
		pszString++;

		IgnoreWhitespace(&pszString);

		if(!strlen(pszString))
		{
			*pError = c_psSemanticDefaultDataTypeInfo[eType].pszName + CPVRTString(" missing arguments");
			return false;
		}

		pszTmp = pszString;
		switch(c_psSemanticDefaultDataTypeInfo[eType].eInternalType)
		{
			case eFloating:
				pDataItem->pfData[i] = (float)strtod(pszString, &pszTmp);
				break;
			case eInteger:
				pDataItem->pnData[i] = (int)strtol(pszString, &pszTmp, 10);
				break;
			case eBoolean:
				if(strncmp(pszString, "true", 4) == 0)
				{
					pDataItem->pbData[i] = true;
					pszTmp = &pszString[4];
				}
				else if(strncmp(pszString, "false", 5) == 0)
				{
					pDataItem->pbData[i] = false;
					pszTmp = &pszString[5];
				}
				break;
		}

		if(pszString == pszTmp)
		{
			size_t n = strcspn(pszString, ",\t ");
			char *pszError = (char *)malloc(n + 1);
			strcpy(pszError, "");
			strncat(pszError, pszString, n);
			*pError = CPVRTString("'") + pszError + "' unexpected for " + c_psSemanticDefaultDataTypeInfo[eType].pszName;
			FREE(pszError);
			return false;
		}
		pszString = pszTmp;

		IgnoreWhitespace(&pszString);
	}

	if(pszString[0] != ')')
	{
		size_t n = strcspn(pszString, "\t )");
		char *pszError = (char *)malloc(n + 1);
		strcpy(pszError, "");
		strncat(pszError, pszString, n);
		*pError = CPVRTString("'") + pszError + "' found when expecting ')' for " + c_psSemanticDefaultDataTypeInfo[eType].pszName;
		FREE(pszError);
		return false;
	}
	pszString++;

	IgnoreWhitespace(&pszString);

	if(strlen(pszString))
	{
		*pError = CPVRTString("'") + pszString + "' unexpected after ')'";
		return false;
	}

	return true;
}

/*!***************************************************************************
 @Function			ConcatenateLinesUntil
 @Output			pszOut		output text
 @Output			nLine		end line number
 @Input				nLine		start line number
 @Input				ppszLines	input text - one array element per line
 @Input				nLimit		number of lines input
 @Input				pszEnd		end string
 @Return			true if successful
 @Description		Outputs a block of text starting from nLine and ending
					when the string pszEnd is found.
*****************************************************************************/
static bool ConcatenateLinesUntil(char *&pszOut, int &nLine, const char * const * const ppszLines, const unsigned int nLimit, const char * const pszEnd)
{
	unsigned int	i, j;
	size_t			nLen;

	nLen = 0;
	for(i = nLine; i < nLimit; ++i)
	{
		if(strcmp(ppszLines[i], pszEnd) == 0)
			break;
		nLen += strlen(ppszLines[i]) + 1;
	}
	if(i == nLimit)
	{
		return false;
	}

	if(nLen)
	{
		++nLen;

		pszOut = (char*)malloc(nLen * sizeof(*pszOut));
		*pszOut = 0;

		for(j = nLine; j < i; ++j)
		{
			strcat(pszOut, ppszLines[j]);
			strcat(pszOut, "\n");
		}
	}
	else
	{
		pszOut = 0;
	}

	nLine = i;
	return true;
}

/*!***************************************************************************
 @Function			CPVRTPFXParser
 @Description		Sets initial values.
*****************************************************************************/
CPVRTPFXParser::CPVRTPFXParser()
{
	m_sHeader.pszVersion = NULL;
	m_sHeader.pszDescription = NULL;
	m_sHeader.pszCopyright = NULL;

	m_szFileName.assign("");

	m_nMaxRenders = 20;			// Arbitrary size for now
	m_nNumRenderPasses = 0;
	m_psRenderPasses = new SPVRTPFXRenderPass[m_nMaxRenders];

	// NOTE: Temp hardcode viewport size
	m_uiViewportWidth = 640;
	m_uiViewportHeight = 480;
}

/*!***************************************************************************
 @Function			~CPVRTPFXParser
 @Description		Frees memory used.
*****************************************************************************/
CPVRTPFXParser::~CPVRTPFXParser()
{
	// FREE header strings
	FREE(m_sHeader.pszVersion);
	FREE(m_sHeader.pszDescription);
	FREE(m_sHeader.pszCopyright);

	// Free render pass info
	delete [] m_psRenderPasses;
}

/*!***************************************************************************
 @Function			Parse
 @Output			pReturnError	error string
 @Return			bool			true for success parsing file
 @Description		Parses a loaded PFX file.
*****************************************************************************/
bool CPVRTPFXParser::Parse(CPVRTString * const pReturnError)
{
	int nEndLine = 0;
	int nHeaderCounter = 0, nTexturesCounter = 0;
	unsigned int i,j,k;

	// Loop through the file
	for(unsigned int nLine=0; nLine < m_psContext->nNumLines; nLine++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[nLine])
			continue;

		if(strcmp("[HEADER]", m_psContext->ppszEffectFile[nLine]) == 0)
		{
			if(nHeaderCounter>0)
			{
				*pReturnError = PVRTStringFromFormattedStr("[HEADER] redefined on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			if(GetEndTag("HEADER", nLine, &nEndLine))
			{
				if(ParseHeader(nLine, nEndLine, pReturnError))
					nHeaderCounter++;
				else
					return false;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing [/HEADER] tag after [HEADER] on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			nLine = nEndLine;
		}
		else if(strcmp("[TEXTURE]", m_psContext->ppszEffectFile[nLine]) == 0)
		{
			if(GetEndTag("TEXTURE", nLine, &nEndLine))
			{
				if(!ParseTexture(nLine, nEndLine, pReturnError))
					return false;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing [/TEXTURE] tag after [TEXTURE] on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			nLine = nEndLine;
		}
		else if(strcmp("[TARGET]", m_psContext->ppszEffectFile[nLine]) == 0)
		{
			if(GetEndTag("TARGET", nLine, &nEndLine))
			{
				if(!ParseTarget(nLine, nEndLine, pReturnError))
					return false;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing [/TARGET] tag after [TARGET] on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			nLine = nEndLine;
		}
		else if(strcmp("[TEXTURES]", m_psContext->ppszEffectFile[nLine]) == 0)
		{
			if(nTexturesCounter>0)
			{
				*pReturnError = PVRTStringFromFormattedStr("[TEXTURES] redefined on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			if(GetEndTag("TEXTURES", nLine, &nEndLine))
			{
				if(ParseTextures(nLine, nEndLine, pReturnError))
					nTexturesCounter++;
				else
					return false;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing [/TEXTURES] tag after [TEXTURES] on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			nLine = nEndLine;
		}
		else if(strcmp("[VERTEXSHADER]", m_psContext->ppszEffectFile[nLine]) == 0)
		{
			if(GetEndTag("VERTEXSHADER", nLine, &nEndLine))
			{
				SPVRTPFXParserShader VertexShader;
				if(ParseShader(nLine, nEndLine, pReturnError, VertexShader, "VERTEXSHADER"))
					m_psVertexShader.Append(VertexShader);
				else
					return false;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing [/VERTEXSHADER] tag after [VERTEXSHADER] on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			nLine = nEndLine;
		}
		else if(strcmp("[FRAGMENTSHADER]", m_psContext->ppszEffectFile[nLine]) == 0)
		{
			if(GetEndTag("FRAGMENTSHADER", nLine, &nEndLine))
			{
				SPVRTPFXParserShader FragShader;
				if(ParseShader(nLine, nEndLine, pReturnError, FragShader, "FRAGMENTSHADER"))
					m_psFragmentShader.Append(FragShader);
				else
					return false;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing [/FRAGMENTSHADER] tag after [FRAGMENTSHADER] on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			nLine = nEndLine;
		}
		else if(strcmp("[EFFECT]", m_psContext->ppszEffectFile[nLine]) == 0)
		{
			if(GetEndTag("EFFECT", nLine, &nEndLine))
			{
				SPVRTPFXParserEffect Effect;
				if(ParseEffect(Effect, nLine, nEndLine, pReturnError))
					m_psEffect.Append(Effect);
				else
					return false;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing [/EFFECT] tag after [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nLine]);
				return false;
			}
			nLine = nEndLine;
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr("'%s' unexpected on line %d\n", m_psContext->ppszEffectFile[nLine], m_psContext->pnFileLineNumber[nLine]);
			return false;
		}
	}

	if(m_psEffect.GetSize() < 1)
	{
		*pReturnError = CPVRTString("No [EFFECT] found. PFX file must have at least one defined.\n");
		return false;
	}

	if(m_psFragmentShader.GetSize() < 1)
	{
		*pReturnError = CPVRTString("No [FRAGMENTSHADER] found. PFX file must have at least one defined.\n");;
		return false;
	}

	if(m_psVertexShader.GetSize() < 1)
	{
		*pReturnError = CPVRTString("No [VERTEXSHADER] found. PFX file must have at least one defined.\n");
		return false;
	}

	for(i = 0; i < m_psEffect.GetSize(); ++i)
	{
		for(j = 0; j < m_psEffect[i].nNumTextures; ++j)
		{
			unsigned int uiTexSize = m_psTexture.GetSize();
			for(k = 0; k < uiTexSize; ++k)
			{
				if(m_psTexture[k]->Name == m_psEffect[i].psTextures[j].Name)
					break;
			}

			if(!uiTexSize || k == uiTexSize)
			{
				*pReturnError = "Error: TEXTURE '" + m_psEffect[i].psTextures[j].Name + "' is not defined in [TEXTURES].\n";
				return false;
			}
		}
	}

	DetermineRenderPassDependencies(pReturnError);
	if(pReturnError->compare(""))
	{
		return false;
	}

	return true;
}

/*!***************************************************************************
 @Function			ParseFromMemory
 @Input				pszScript		PFX script
 @Output			pReturnError	error string
 @Return			EPVRTError		PVR_SUCCESS for success parsing file
									PVR_FAIL if file doesn't exist or is invalid
 @Description		Parses a PFX script from memory.
*****************************************************************************/
EPVRTError CPVRTPFXParser::ParseFromMemory(const char * const pszScript, CPVRTString * const pReturnError)
{
	CPVRTPFXParserReadContext	context;
	char			pszLine[512];
	const char		*pszEnd, *pszCurr;
	int				nLineCounter;
	unsigned int	nLen;
	unsigned int	nReduce;
	bool			bDone;

	if(!pszScript)
		return PVR_FAIL;

	m_psContext = &context;

	// Find & process each line
	nLineCounter	= 0;
	bDone			= false;
	pszCurr			= pszScript;
	while(!bDone)
	{
		nLineCounter++;

		while(*pszCurr == '\r')
			++pszCurr;

		// Find length of line
		pszEnd = strchr(pszCurr, '\n');
		if(pszEnd)
		{
			nLen = (unsigned int)(pszEnd - pszCurr);
		}
		else
		{
			nLen = (unsigned int)strlen(pszCurr);
			bDone = true;
		}

		nReduce = 0; // Tells how far to go back because of '\r'.
		while(nLen - nReduce > 0 && pszCurr[nLen - 1 - nReduce] == '\r')
			nReduce++;

		// Ensure pszLine will not be not overrun
		if(nLen+1-nReduce > sizeof(pszLine) / sizeof(*pszLine))
			nLen = sizeof(pszLine) / sizeof(*pszLine) - 1 + nReduce;

		// Copy line into pszLine
		strncpy(pszLine, pszCurr, nLen - nReduce);
		pszLine[nLen - nReduce] = 0;
		pszCurr += nLen + 1;

		_ASSERT(strchr(pszLine, '\r') == 0);
		_ASSERT(strchr(pszLine, '\n') == 0);

		// Ignore comments
		char *tmp = strstr(pszLine, "//");
		if(tmp != NULL)	*tmp = '\0';

		// Reduce whitespace to one character.
		ReduceWhitespace(pszLine);

		// Store the line, even if blank lines (to get correct errors from GLSL compiler).
		if(m_psContext->nNumLines < m_psContext->nMaxLines)
		{
			m_psContext->pnFileLineNumber[m_psContext->nNumLines] = nLineCounter;
			m_psContext->ppszEffectFile[m_psContext->nNumLines] = (char *)malloc((strlen(pszLine) + 1) * sizeof(char));
			strcpy(m_psContext->ppszEffectFile[m_psContext->nNumLines], pszLine);
			m_psContext->nNumLines++;
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr("Too many lines of text in file (maximum is %d)\n", m_psContext->nMaxLines);
			return PVR_FAIL;
		}
	}

	return Parse(pReturnError) ? PVR_SUCCESS : PVR_FAIL;
}

/*!***************************************************************************
 @Function			ParseFromFile
 @Input				pszFileName		PFX file name
 @Output			pReturnError	error string
 @Return			EPVRTError		PVR_SUCCESS for success parsing file
									PVR_FAIL if file doesn't exist or is invalid
 @Description		Reads the PFX file and calls the parser.
*****************************************************************************/
EPVRTError CPVRTPFXParser::ParseFromFile(const char * const pszFileName, CPVRTString * const pReturnError)
{
	CPVRTResourceFile PfxFile(pszFileName);
	if (!PfxFile.IsOpen())
	{
		*pReturnError = CPVRTString("Unable to open file ") + pszFileName;
		return PVR_FAIL;
	}

	CPVRTString PfxFileString;
	const char* pPfxData = (const char*) PfxFile.DataPtr();

	// Is our shader resource file data null terminated?
	if(pPfxData[PfxFile.Size()-1] != '\0')
	{
		// If not create a temporary null-terminated string
		PfxFileString.assign(pPfxData, PfxFile.Size());
		pPfxData = PfxFileString.c_str();
	}

	m_szFileName.assign(pszFileName);

	return ParseFromMemory(pPfxData, pReturnError);
}

/*!***************************************************************************
 @Function			SetViewportSize
 @Input				uiWidth				New viewport width
 @Input				uiHeight			New viewport height
 @Return			bool				True on success
 @Description		Allows the current viewport size to be set. This value
					is used for calculating relative texture resolutions
*****************************************************************************/
bool CPVRTPFXParser::SetViewportSize(unsigned int uiWidth, unsigned int uiHeight)
{
	if(uiWidth > 0 && uiHeight > 0)
	{
		m_uiViewportWidth = uiWidth;
		m_uiViewportHeight = uiHeight;
		return true;
	}
	else
	{
		return false;
	}
}
/*!***************************************************************************
@Function		RetrieveRenderPassDependencies
@Output			aRequiredRenderPasses
@Output			aszActiveEffectStrings
@Return			bool	
@Description	Returns a list of dependencies associated with the pass.
*****************************************************************************/
bool CPVRTPFXParser::RetrieveRenderPassDependencies(CPVRTArray<SPVRTPFXRenderPass*> &aRequiredRenderPasses, CPVRTArray<CPVRTString> &aszActiveEffectStrings)
{
	unsigned int ui(0), uj(0), uk(0), ul(0);
	const SPVRTPFXParserEffect* pTempEffect(NULL);
	
	if(aRequiredRenderPasses.GetSize() > 0)
	{
		/* aRequiredRenderPasses should be empty when it is passed in */
		return false;
	}

	for(ui = 0; ui < (unsigned int)aszActiveEffectStrings.GetSize(); ++ui)
	{
		if(aszActiveEffectStrings[ui].compare("") == 0)
		{
			// Empty strings are not valid
			return false;
		}

		// Find the specified effect
		for(uj = 0, pTempEffect = NULL; uj < (unsigned int)m_psEffect.GetSize(); ++uj)
		{
			if(aszActiveEffectStrings[ui].compare(m_psEffect[uj].pszName) == 0)
			{
				// Effect found
				pTempEffect = &m_psEffect[uj];
				break;
			}
		}

		if(pTempEffect == NULL)
		{
			// Effect not found
			return false;
		}
		
		for(uj = 0; uj < m_renderPassSkipGraph.GetNumNodes(); ++uj)
		{
			if(m_renderPassSkipGraph[uj]->pEffect == pTempEffect)
			{
				m_renderPassSkipGraph.RetreiveSortedDependencyList(aRequiredRenderPasses, uj);
				return true;
			}
		}

		/*
			The effect wasn't a post-process. Check to see if it has any non-post-process dependencies,
			e.g. RENDER CAMERA textures.
		*/
		for(uj = 0; uj < (unsigned int)m_psEffect.GetSize(); ++uj)
		{
			if(0 == aszActiveEffectStrings[ui].compare(m_psEffect[uj].pszName))
			{
				for(uk = 0; uk < m_psEffect[uj].nNumTextures;++uk)
				{
					for(ul = 0; ul < m_nNumRenderPasses; ++ul)
					{
						if(m_psRenderPasses[ul].pTexture->Name == m_psEffect[uj].psTextures[uk].Name)
						{
							aRequiredRenderPasses.Append(&m_psRenderPasses[ul]);
						}
					}
				}
				return true;
			}
		}
	}

	return false;
}
/*!***************************************************************************
 @Function			GetEndTag
 @Input				pszTagName		tag name
 @Input				nStartLine		start line
 @Output			pnEndLine		line end tag found
 @Return			true if tag found
 @Description		Searches for end tag pszTagName from line nStartLine.
					Returns true and outputs the line number of the end tag if
					found, otherwise returning false.
*****************************************************************************/
bool CPVRTPFXParser::GetEndTag(const char* pszTagName, int nStartLine, int *pnEndLine)
{
	char pszEndTag[100];
	strcpy(pszEndTag, "[/");
	strcat(pszEndTag, pszTagName);
	strcat(pszEndTag, "]");

	for(unsigned int i = nStartLine; i < m_psContext->nNumLines; i++)
	{
		if(strcmp(pszEndTag, m_psContext->ppszEffectFile[i]) == 0)
		{
			*pnEndLine = i;
			return true;
		}
	}

	return false;
}

/*!***************************************************************************
 @Function			ReduceWhitespace
 @Output			line		output text
 @Input				line		input text
 @Description		Reduces all white space characters in the string to one
					blank space.
*****************************************************************************/
void CPVRTPFXParser::ReduceWhitespace(char *line)
{


	// convert tabs and newlines to ' '
	char *tmp = strpbrk (line, "\t\n");
	while(tmp != NULL)
	{
		*tmp = ' ';
		tmp = strpbrk (line, "\t\n");
	}

	// remove all whitespace at start
	while(line[0] == ' ')
	{
		// move chars along to omit whitespace
		int counter = 0;
		do{
			line[counter] = line[counter+1];
			counter++;
		}while(line[counter] != '\0');
	}

	// step through chars of line remove multiple whitespace
	for(int i=0; i < (int)strlen(line); i++)
	{
		// whitespace found
		if(line[i] == ' ')
		{
			// count number of whitespace chars
			int numWhiteChars = 0;
			while(line[i+1+numWhiteChars] == ' ')
			{
				numWhiteChars++;
			}

			// multiple whitespace chars found
			if(numWhiteChars>0)
			{
				// move chars along to omit whitespace
				int counter=1;
				while(line[i+counter] != '\0')
				{
					line[i+counter] = line[i+numWhiteChars+counter];
					counter++;
				}
			}
		}
	}

	// If there is no string then do not remove terminating white symbols
	if(!strlen(line))
	    return;

	// remove all whitespace from end
	while(line[strlen(line)-1] == ' ')
	{
		// move chars along to omit whitespace
		line[strlen(line)-1] = '\0';
	}
}

/*!***************************************************************************
 @Function			FindParameter
 @Output
 @Input
 @Description		Finds the parameter after the specified delimiting character and
					returns the parameter as a string. An empty string is returned
					if a parameter cannot be found

*****************************************************************************/
CPVRTString CPVRTPFXParser::FindParameter(char *aszSourceString, const CPVRTString &parameterTag, const CPVRTString &delimiter)
{
	CPVRTString returnString("");
	char* aszTagStart = strstr(aszSourceString, parameterTag.c_str());

	// Tag was found, so search for parameter
	if(aszTagStart)
	{
		char* aszDelimiterStart = strstr(aszTagStart, delimiter.c_str());
		char* aszSpaceStart = strstr(aszTagStart, " ");

		// Delimiter found
		if(aszDelimiterStart && (!aszSpaceStart ||(aszDelimiterStart < aszSpaceStart)))
		{
			// Create a string from the delimiter to the next space
			size_t strCount(strcspn(aszDelimiterStart, " "));
			aszDelimiterStart++;	// Skip =
			returnString.assign(aszDelimiterStart, strCount-1);
		}
	}

	return returnString;
}

/*!***************************************************************************
@Function		ReadStringToken
@Input			pszSource			Parameter string to process
@Output			output				Processed string
@Output			ErrorStr			String containing errors
@Return								Returns true on success
@Description	Processes the null terminated char array as if it's a
				formatted string array. Quote marks are determined to be
				start and end of strings. If no quote marks are found the
				string is delimited by whitespace.
*****************************************************************************/
bool CPVRTPFXParser::ReadStringToken(char* pszSource, CPVRTString& output, CPVRTString &ErrorStr, int i, const char* pCaller)
{
	if(*pszSource == '\"')		// Quote marks. Continue parsing until end mark or NULL
	{	
		pszSource++;		// Skip past first quote
		while(*pszSource != '\"')
		{
			if(*pszSource == '\0')
			{
				ErrorStr = PVRTStringFromFormattedStr("Incomplete argument in [%s] on line %d: %s\n", pCaller,m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
				return false;
			}

			output.push_back(*pszSource);
			pszSource++;
		}

		pszSource++;		// Skip past final quote.
	}
	else		// No quotes. Read until space
	{
		pszSource = strtok(pszSource, DELIM_TOKENS NEWLINE_TOKENS);
		output = pszSource;

		pszSource += strlen(pszSource);
	}

	// Check that there's nothing left on this line
	pszSource = strtok(pszSource, NEWLINE_TOKENS);
	if(pszSource)
	{
		ErrorStr = PVRTStringFromFormattedStr("Unknown keyword '%s' in [%s] on line %d: %s\n", pszSource, pCaller, m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
		return false;
	}

	return true;
}

/*!***************************************************************************
 @Function			ParseHeader
 @Input				nStartLine		start line number
 @Input				nEndLine		end line number
 @Output			pReturnError	error string
 @Return			bool			true if parse is successful
 @Description		Parses the HEADER section of the PFX file.
*****************************************************************************/
bool CPVRTPFXParser::ParseHeader(int nStartLine, int nEndLine, CPVRTString * const pReturnError)
{
	for(int i = nStartLine+1; i < nEndLine; i++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[i])
			continue;

		char *str = strtok (m_psContext->ppszEffectFile[i]," ");
		if(str != NULL)
		{
			if(strcmp(str, "VERSION") == 0)
			{
				str += (strlen(str)+1);
				m_sHeader.pszVersion = (char *)malloc((strlen(str) + 1) * sizeof(char));
				strcpy(m_sHeader.pszVersion, str);
			}
			else if(strcmp(str, "DESCRIPTION") == 0)
			{
				str += (strlen(str)+1);
				m_sHeader.pszDescription = (char *)malloc((strlen(str) + 1) * sizeof(char));
				strcpy(m_sHeader.pszDescription, str);
			}
			else if(strcmp(str, "COPYRIGHT") == 0)
			{
				str += (strlen(str)+1);
				m_sHeader.pszCopyright = (char *)malloc((strlen(str) + 1) * sizeof(char));
				strcpy(m_sHeader.pszCopyright, str);
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [HEADER] on line %d\n", str, m_psContext->pnFileLineNumber[i]);
				return false;
			}
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr("Missing arguments in [HEADER] on line %d : %s\n", m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
			return false;
		}
	}

	// initialise empty strings
	if(m_sHeader.pszVersion == NULL)
	{
		m_sHeader.pszVersion = (char *)malloc(sizeof(char));
		strcpy(m_sHeader.pszVersion, "");
	}
	if(m_sHeader.pszDescription == NULL)
	{
		m_sHeader.pszDescription = (char *)malloc(sizeof(char));
		strcpy(m_sHeader.pszDescription, "");
	}
	if(m_sHeader.pszCopyright == NULL)
	{
		m_sHeader.pszCopyright = (char *)malloc(sizeof(char));
		strcpy(m_sHeader.pszCopyright, "");
	}

	return true;
}

/*!***************************************************************************
@Function			ParseGenericSurface
@Input				nStartLine		start line number
@Input				nEndLine		end line number
@Output				uiWrapS			
@Output				uiWrapT			
@Output				uiWrapR			
@Output				uiMin			
@Output				uiMag			
@Output				uiMip			
@Output				pReturnError	error string
@Return				bool			true if parse is successful
@Description		Parses generic data from TARGET and TEXTURE blocks. Namely
					wrapping and filter commands.
*****************************************************************************/
bool CPVRTPFXParser::ParseGenericSurface(int nStartLine, int nEndLine, SPVRTPFXParserTexture& Params, CPVRTArray<CPVRTHash>& KnownCmds, 
										 const char* pCaller, CPVRTString * const pReturnError)
{
	const unsigned int INVALID_TYPE = 0xAC1DBEEF;
	
	enum eCmd
	{
		eCmds_Min,
		eCmds_Mag,
		eCmds_Mip,
		eCmds_WrapS,
		eCmds_WrapT,
		eCmds_WrapR,
		eCmds_Filter,
		eCmds_Wrap,
		eCmds_Resolution,
		eCmds_Surface,

		eCmds_Size
	};

	const CPVRTHash Cmds[] =
	{
		"MINIFICATION",			// eCmds_Min
		"MAGNIFICATION",		// eCmds_Mag
		"MIPMAP",				// eCmds_Mip
		"WRAP_S",				// eCmds_WrapS
		"WRAP_T",				// eCmds_WrapT
		"WRAP_R",				// eCmds_WrapR
		"FILTER",				// eCmds_Filter
		"WRAP",					// eCmds_Wrap
		"RESOLUTION",			// eCmds_Resolution
		"SURFACETYPE",			// eCmds_Surface
	};

	struct SSurfacePair
	{
		CPVRTHash Name;
		PVRTPixelType eType;
		unsigned int BufferType;
	};

	const SSurfacePair SurfacePairs[] = 
	{
		{ "RGBA8888",	OGL_RGBA_8888,	PVRPFXTEX_COLOUR },
		{ "RGBA4444",	OGL_RGBA_4444,	PVRPFXTEX_COLOUR },
		{ "RGB888",		OGL_RGB_888,	PVRPFXTEX_COLOUR },
		{ "RGB565",		OGL_RGB_565,	PVRPFXTEX_COLOUR },		
		{ "INTENSITY8",	OGL_I_8,		PVRPFXTEX_COLOUR },
		{ "DEPTH24",	OGL_RGB_888,	PVRPFXTEX_DEPTH },
		{ "DEPTH16",	OGL_RGB_565,	PVRPFXTEX_DEPTH },
		{ "DEPTH8",		OGL_I_8,		PVRPFXTEX_DEPTH },
	};
	const unsigned int uiNumSurfTypes = sizeof(SurfacePairs) / sizeof(SurfacePairs[0]);

	for(int i = nStartLine+1; i < nEndLine; i++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[i])
			continue;

		// Need to make a copy so we can use strtok and not affect subsequent parsing
		size_t lineLen = strlen(m_psContext->ppszEffectFile[i]);
		char* pBlockCopy = new char[lineLen+1];
		strcpy(pBlockCopy, m_psContext->ppszEffectFile[i]);

		char *str = strtok (pBlockCopy, NEWLINE_TOKENS DELIM_TOKENS);
		if(!str)
		{
			return false;		
		}

		CPVRTHash Cmd(str);
		const char** ppFilters  = NULL;
		unsigned int uiNumFlags = 0;
		bool bKnown = false;

		// --- Verbose filtering flags
		if(Cmd == Cmds[eCmds_Min] || Cmd == Cmds[eCmds_Mag] || Cmd == Cmds[eCmds_Mip])
		{
			ppFilters = c_ppszFilters;
			uiNumFlags = 3;
			bKnown     = true;
		}
		// --- Verbose wrapping flags
		else if(Cmd == Cmds[eCmds_WrapS] || Cmd == Cmds[eCmds_WrapT] || Cmd == Cmds[eCmds_WrapR])
		{
			ppFilters = c_ppszWraps;
			uiNumFlags = 3;
			bKnown     = true;
		}
		// --- Inline filtering flags
		else if(Cmd == Cmds[eCmds_Filter])
		{
			char* pszRemaining = strtok(NULL, NEWLINE_TOKENS DELIM_TOKENS);
			if(!pszRemaining)
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing FILTER arguments in [%s] on line %d: %s\n", pCaller, m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
				return false;
			}

			unsigned int* pFlags[3] =
			{
				&Params.nMin,
				&Params.nMag,
				&Params.nMIP,
			};

			if(!ParseTextureFlags(pszRemaining, pFlags, 3, c_ppszFilters, eFilter_Size, pReturnError, i))
			{
				return false;
			}

			bKnown     = true;
		}
		// --- Inline wrapping flags
		else if(Cmd == Cmds[eCmds_Wrap])
		{
			char* pszRemaining = strtok(NULL, NEWLINE_TOKENS DELIM_TOKENS);
			if(!pszRemaining)
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing WRAP arguments in [%s] on line %d: %s\n", pCaller, m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
				return false;
			}

			unsigned int* pFlags[3] =
			{
				&Params.nWrapS,
				&Params.nWrapT,
				&Params.nWrapR,
			};

			if(!ParseTextureFlags(pszRemaining, pFlags, 3, c_ppszWraps, eWrap_Size, pReturnError, i))
			{
				return false;
			}

			bKnown     = true;
		}
		// --- Resolution
		else if(Cmd == Cmds[eCmds_Resolution])
		{
			char* pszRemaining;

			unsigned int* uiVals[2] = { &Params.uiWidth, &Params.uiHeight };

			// There should be precisely TWO arguments for resolution (width and height)
			for(unsigned int uiIndex = 0; uiIndex < 2; ++uiIndex)
			{
				pszRemaining = strtok(NULL, DELIM_TOKENS NEWLINE_TOKENS);
				if(!pszRemaining)
				{
					*pReturnError = PVRTStringFromFormattedStr("Missing RESOLUTION argument(s) (requires width AND height) in [TARGET] on line %d\n", m_psContext->pnFileLineNumber[i]);
					return false;
				}

				int val = atoi(pszRemaining);

				if( (val == 0 && *pszRemaining != '0')			// Make sure they haven't explicitly set the value to be 0 as this might be a valid use-case.
					||  (val < 0))
				{
					*pReturnError = PVRTStringFromFormattedStr("Invalid RESOLUTION argument \"%s\" in [TEXTURE] on line %d\n", pszRemaining, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				*(uiVals[uiIndex]) = (unsigned int)val;
			}

			bKnown     = true;
		}
		// --- Surface type
		else if(Cmd == Cmds[eCmds_Surface])
		{
			char* pszRemaining = strtok(NULL, NEWLINE_TOKENS DELIM_TOKENS);
			if(!pszRemaining)
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing SURFACETYPE arguments in [TARGET] on line %d\n", m_psContext->pnFileLineNumber[i]);
				return false;
			}

			CPVRTHash hashType(pszRemaining);
			for(unsigned int uiIndex = 0; uiIndex < uiNumSurfTypes; ++uiIndex)
			{
				if(hashType == SurfacePairs[uiIndex].Name)
				{
					Params.uiFlags =  SurfacePairs[uiIndex].eType | SurfacePairs[uiIndex].BufferType;
					break;
				}
			}

			bKnown     = true;
		}

		// Valid Verbose command
		if(ppFilters)
		{
			char* pszRemaining = strtok(NULL, NEWLINE_TOKENS DELIM_TOKENS);
			if(!pszRemaining)
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing arguments in [%s] on line %d: %s\n", pCaller, m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
				return false;
			}

			unsigned int Type = INVALID_TYPE;
			for(unsigned int uiIndex = 0; uiIndex < 3; ++uiIndex)
			{
				if(strcmp(pszRemaining, ppFilters[uiIndex]) == 0)	
				{
					Type = uiIndex;			// Yup, it's valid.
					break;
				}
			}

			// Tell the user it's invalid.
			if(Type == INVALID_TYPE)
			{
				*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [%s] on line %d: %s\n", pszRemaining, pCaller, m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
				return false;
			}

			if(Cmd == Cmds[eCmds_Min])			Params.nMin = Type;		
			else if(Cmd == Cmds[eCmds_Mag])		Params.nMag = Type;	
			else if(Cmd == Cmds[eCmds_Mip])		Params.nMIP = Type;	
			else if(Cmd == Cmds[eCmds_WrapR])	Params.nWrapR = Type;	
			else if(Cmd == Cmds[eCmds_WrapS])	Params.nWrapS = Type;	
			else if(Cmd == Cmds[eCmds_WrapT])	Params.nWrapT = Type;
		}

		if(bKnown)
		{
			KnownCmds.Append(Cmd);

			// Make sure nothing else exists on the line that hasn't been parsed.
			char* pszRemaining = strtok(NULL, NEWLINE_TOKENS);
			if(pszRemaining)
			{
				*pReturnError = PVRTStringFromFormattedStr("Unexpected keyword '%s' in [%s] on line %d: %s\n", pszRemaining, pCaller, m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
				return false;
			}
		}	

		delete [] pBlockCopy;
	}

	return true;
}

/*!***************************************************************************
@Function			ParseTexture
@Input				nStartLine		start line number
@Input				nEndLine		end line number
@Output				pReturnError	error string
@Return				bool			true if parse is successful
@Description		Parses the TEXTURE section of the PFX file.
*****************************************************************************/
bool CPVRTPFXParser::ParseTexture(int nStartLine, int nEndLine, CPVRTString * const pReturnError)
{
	enum eTextureCmds
	{
		eTextureCmds_Name,
		eTextureCmds_Path,
		eTextureCmds_View,
		eTextureCmds_Camera,

		eTextureCmds_Size
	};

	const CPVRTHash TextureCmds[] =
	{
		"NAME",				// eTextureCmds_Name
		"PATH",				// eTextureCmds_Path
		"VIEW",				// eTextureCmds_View
		"CAMERA",			// eTextureCmds_Camera
	};

	SPVRTPFXParserTexture TexDesc;
	TexDesc.nMin = eFilter_Default;
	TexDesc.nMag = eFilter_Default;
	TexDesc.nMIP = eFilter_MipDefault;
	TexDesc.nWrapS = eWrap_Default;
	TexDesc.nWrapT = eWrap_Default;
	TexDesc.nWrapR = eWrap_Default;
	TexDesc.uiWidth  = VIEWPORT_SIZE;
	TexDesc.uiHeight = VIEWPORT_SIZE;
	TexDesc.uiFlags  = OGL_RGBA_8888 | PVRPFXTEX_COLOUR;

	CPVRTArray<CPVRTHash> KnownCmds;
	if(!ParseGenericSurface(nStartLine, nEndLine, TexDesc, KnownCmds, "TEXTURE", pReturnError))
		return false;

	CPVRTString texName, filePath, viewName;
	for(int i = nStartLine+1; i < nEndLine; i++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[i])
			continue;

		char *str = strtok (m_psContext->ppszEffectFile[i], NEWLINE_TOKENS DELIM_TOKENS);
		if(!str)
		{
			*pReturnError = PVRTStringFromFormattedStr("Missing arguments in [TEXTURE] on line %d: %s\n", m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
			return false;
		}

		CPVRTHash texCmd(str);
		// --- Texture Name
		if(texCmd == TextureCmds[eTextureCmds_Name])
		{
			char* pszRemaining = strtok(NULL, NEWLINE_TOKENS DELIM_TOKENS);
			if(!pszRemaining)
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing NAME arguments in [TEXTURE] on line %d: %s\n", m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
				return false;
			}

			texName = pszRemaining;
		}
		// --- Texture Path
		else if(texCmd == TextureCmds[eTextureCmds_Path])
		{
			char* pszRemaining = strtok(NULL, NEWLINE_TOKENS);
			if(!pszRemaining)
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing PATH arguments in [TEXTURE] on line %d: %s\n", m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
				return false;
			}

			if(!ReadStringToken(pszRemaining, filePath, *pReturnError, i, "TEXTURE"))
			{
				return false;
			}
		}
		// --- View/Camera Name
		else if(texCmd == TextureCmds[eTextureCmds_View] || texCmd == TextureCmds[eTextureCmds_Camera])
		{
			char* pszRemaining = strtok(NULL, NEWLINE_TOKENS);		// String component. Get the rest of the line.
			if(!pszRemaining || strlen(pszRemaining) == 0)
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing VIEW argument in [TEXTURE] on line %d: %s\n", m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
				return false;
			}

			if(!ReadStringToken(pszRemaining, viewName, *pReturnError, i, "TEXTURE"))
			{
				return false;
			}
		}
		else if(KnownCmds.Contains(texCmd))
		{
			// Remove from 'unknown' list.
			for(unsigned int uiIndex = 0; uiIndex < KnownCmds.GetSize(); ++uiIndex)
			{
				if(KnownCmds[uiIndex] == texCmd)
				{
					KnownCmds.Remove(uiIndex);
					break;
				}
			}

			continue;		// This line has already been processed.
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [TEXTURE] on line %d: %s\n", str, m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
			return false;
		}

		char* pszRemaining = strtok(NULL, NEWLINE_TOKENS);
		if(pszRemaining)
		{
			*pReturnError = PVRTStringFromFormattedStr("Unexpected keyword '%s' in [TEXTURE] on line %d: %s\n", pszRemaining, m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
			return false;
		}
	}

	if(texName.empty())
	{
		*pReturnError = PVRTStringFromFormattedStr("No NAME tag specified in [TEXTURE] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);
		return false;
	}
	if(!filePath.empty() && !viewName.empty())
	{
		*pReturnError = PVRTStringFromFormattedStr("Both PATH and VIEW tags specified in [TEXTURE] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);
		return false;
	}
	if(filePath.empty() && viewName.empty())
	{
		*pReturnError = PVRTStringFromFormattedStr("No PATH or VIEW tag specified in [TEXTURE] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);
		return false;
	}

	bool bRTT = (viewName.empty() ? false : true);
	if(bRTT)
	{
		filePath = texName;									// RTT doesn't have a physical file.
	}

	// Create a new texture and copy over the vals.
	SPVRTPFXParserTexture* pTex = new SPVRTPFXParserTexture();
	pTex->Name				= texName;
	pTex->FileName			= filePath;
	pTex->bRenderToTexture	= bRTT;
	pTex->nMin				= TexDesc.nMin;
	pTex->nMag				= TexDesc.nMag;
	pTex->nMIP				= TexDesc.nMIP;
	pTex->nWrapS			= TexDesc.nWrapS;
	pTex->nWrapT			= TexDesc.nWrapT;
	pTex->nWrapR			= TexDesc.nWrapR;
	pTex->uiWidth			= TexDesc.uiWidth;
	pTex->uiHeight			= TexDesc.uiHeight;
	pTex->uiFlags			= TexDesc.uiFlags;
	m_psTexture.Append(pTex);

	if(bRTT)
	{
		m_psRenderPasses[m_nNumRenderPasses].SemanticName = texName;

		if(viewName == c_pszCurrentView)
		{
			m_psRenderPasses[m_nNumRenderPasses].eViewType	 = eVIEW_CURRENT;
		}
		else
		{
			m_psRenderPasses[m_nNumRenderPasses].eViewType	 = eVIEW_POD_CAMERA;
			m_psRenderPasses[m_nNumRenderPasses].NodeName	 = viewName;
		}

		m_psRenderPasses[m_nNumRenderPasses].eRenderPassType = eCAMERA_PASS;			// Textures are always 'camera' passes

		// Set render pass texture to the newly created texture.
		m_psRenderPasses[m_nNumRenderPasses].pTexture		= pTex;
		m_psRenderPasses[m_nNumRenderPasses].uiFormatFlags  = TexDesc.uiFlags;

		m_nNumRenderPasses++;
	}
	
	return true;
}

/*!***************************************************************************
@Function			ParseTarget
@Input				nStartLine		start line number
@Input				nEndLine		end line number
@Output				pReturnError	error string
@Return				bool			true if parse is successful
@Description		Parses the TARGET section of the PFX file.
*****************************************************************************/
bool CPVRTPFXParser::ParseTarget(int nStartLine, int nEndLine, CPVRTString * const pReturnError)
{
	enum eTextureCmds
	{
		eTargetCmds_Name,
		eTargetCmds_Size
	};

	const CPVRTHash TextureCmds[] =
	{
		"NAME",				// eTextureCmds_Name
	};
	
	CPVRTString targetName;
	SPVRTPFXParserTexture TexDesc;
	TexDesc.nMin = eFilter_Default;
	TexDesc.nMag = eFilter_Default;
	TexDesc.nMIP = eFilter_MipDefault;
	TexDesc.nWrapS = eWrap_Default;
	TexDesc.nWrapT = eWrap_Default;
	TexDesc.nWrapR = eWrap_Default;
	TexDesc.uiWidth  = VIEWPORT_SIZE;
	TexDesc.uiHeight = VIEWPORT_SIZE;
	TexDesc.uiFlags  = OGL_RGBA_8888 | PVRPFXTEX_COLOUR;

	CPVRTArray<CPVRTHash> KnownCmds;
	if(!ParseGenericSurface(nStartLine, nEndLine, TexDesc, KnownCmds, "TARGET", pReturnError))
		return false;
	
	for(int i = nStartLine+1; i < nEndLine; i++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[i])
			continue;

		char *str = strtok (m_psContext->ppszEffectFile[i], NEWLINE_TOKENS DELIM_TOKENS);
		if(!str)
		{
			*pReturnError = PVRTStringFromFormattedStr("Missing arguments in [TARGET] on line %d\n", m_psContext->pnFileLineNumber[i]);
			return false;
		}

		CPVRTHash texCmd(str);
		// --- Target Name
		if(texCmd == TextureCmds[eTargetCmds_Name])
		{
			char* pszRemaining = strtok(NULL, NEWLINE_TOKENS DELIM_TOKENS);
			if(!pszRemaining)
			{
				*pReturnError = PVRTStringFromFormattedStr("Missing NAME arguments in [TARGET] on line %d\n", m_psContext->pnFileLineNumber[i]);
				return false;
			}

			targetName = pszRemaining;
		}
		else if(KnownCmds.Contains(texCmd))
		{
			// Remove from 'unknown' list.
			for(unsigned int uiIndex = 0; uiIndex < KnownCmds.GetSize(); ++uiIndex)
			{
				if(KnownCmds[uiIndex] == texCmd)
				{
					KnownCmds.Remove(uiIndex);
					break;
				}
			}

			continue;		// This line has already been processed.
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [TARGET] on line %d\n", str, m_psContext->pnFileLineNumber[i]);
			return false;
		}

		char* pszRemaining = strtok(NULL, NEWLINE_TOKENS);
		if(pszRemaining)
		{
			*pReturnError = PVRTStringFromFormattedStr("Unexpected keyword '%s' in [TARGET] on line %d\n", pszRemaining, m_psContext->pnFileLineNumber[i]);
			return false;
		}
	}

	// Create a new texture and copy over the vals.
	SPVRTPFXParserTexture* pTex = new SPVRTPFXParserTexture();
	pTex->Name				= targetName;
	pTex->FileName			= targetName;
	pTex->bRenderToTexture	= true;
	pTex->nMin				= TexDesc.nMin;
	pTex->nMag				= TexDesc.nMag;
	pTex->nMIP				= TexDesc.nMIP;
	pTex->nWrapS			= TexDesc.nWrapS;
	pTex->nWrapT			= TexDesc.nWrapT;
	pTex->nWrapR			= TexDesc.nWrapR;
	pTex->uiWidth			= TexDesc.uiWidth;
	pTex->uiHeight			= TexDesc.uiHeight;
	pTex->uiFlags			= TexDesc.uiFlags;
	m_psTexture.Append(pTex);

	// Copy to render pass struct
	m_psRenderPasses[m_nNumRenderPasses].SemanticName		= targetName;
	m_psRenderPasses[m_nNumRenderPasses].eViewType			= eVIEW_NONE;
	m_psRenderPasses[m_nNumRenderPasses].eRenderPassType	= ePOSTPROCESS_PASS;			// Targets are always post-process passes.
	m_psRenderPasses[m_nNumRenderPasses].pTexture			= pTex;
	m_psRenderPasses[m_nNumRenderPasses].uiFormatFlags		= TexDesc.uiFlags;

	m_nNumRenderPasses++;

	return true;
}

/*!***************************************************************************
 @Function			ParseTextures		** DEPRECATED **
 @Input				nStartLine		start line number
 @Input				nEndLine		end line number
 @Output			pReturnError	error string
 @Return			bool			true if parse is successful
 @Description		Parses the TEXTURE section of the PFX file.
*****************************************************************************/
bool CPVRTPFXParser::ParseTextures(int nStartLine, int nEndLine, CPVRTString * const pReturnError)
{
	char *pszName(NULL), *pszFile(NULL), *pszKeyword(NULL);
	char *pszRemaining(NULL), *pszTemp(NULL);
	bool bReturnVal(false);

	for(int i = nStartLine+1; i < nEndLine; i++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[i])
			continue;

		char *str = strtok (m_psContext->ppszEffectFile[i]," ");
		if(str != NULL)
		{
			// Set defaults
			unsigned int	uiMin(eFilter_Default), uiMag(eFilter_Default), uiMip(eFilter_MipDefault);
			unsigned int	uiWrapS(eWrap_Default), uiWrapT(eWrap_Default), uiWrapR(eWrap_Default);
			unsigned int	uiFlags = 0;

			unsigned int uiWidth	= CPVRTPFXParser::VIEWPORT_SIZE;
			unsigned int uiHeight	= CPVRTPFXParser::VIEWPORT_SIZE;

			// Reset variables
			FREE(pszName)		pszName = NULL;
			FREE(pszFile)		pszFile = NULL;
			FREE(pszKeyword)	pszKeyword = NULL;
			FREE(pszTemp)		pszTemp = NULL;
			pszRemaining		= NULL;

			// Compare against all valid keywords
			if((strcmp(str, "FILE") != 0) && (strcmp(str, "RENDER") != 0))
			{
				*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [TEXTURES] on line %d\n", str, m_psContext->pnFileLineNumber[i]);
				goto fail_release_return;
			}

#if 1
			if((strcmp(str, "RENDER") == 0))
			{
				*pReturnError = PVRTStringFromFormattedStr("RENDER tag no longer supported in [TEXTURES] block. Use new [TARGET] block instead\n");
				goto fail_release_return;
			}
#endif

			pszKeyword = (char *)malloc( ((int)strlen(str)+1) * sizeof(char));
			strcpy(pszKeyword, str);

			str = strtok (NULL, " ");
			if(str != NULL)
			{
				pszName = (char *)malloc( ((int)strlen(str)+1) * sizeof(char));
				strcpy(pszName, str);
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Texture name missing in [TEXTURES] on line %d: %s\n", m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
				goto fail_release_return;
			}

			/*
				The pszRemaining string is used to look for remaining flags.
				This has the advantage of allowing flags to be order independent
				and makes it easier to ommit some flags, but still pick up others
				(the previous method made it diffifult to retrieve filtering info
				if flags before it were missing)
			*/
			pszRemaining  = strtok(NULL, "\n");

			if(pszRemaining == NULL)
			{
				*pReturnError = PVRTStringFromFormattedStr("Incomplete definition in [TEXTURES] on line %d: %s\n", m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
				goto fail_release_return;
			}
			else if(strcmp(pszKeyword, "FILE") == 0)
			{
				pszTemp = (char *)malloc( ((int)strlen(pszRemaining)+1) * sizeof(char));
				strcpy(pszTemp, pszRemaining);
				str = strtok (pszTemp, " ");

				if(str != NULL)
				{
					pszFile = (char *)malloc( ((int)strlen(str)+1) * sizeof(char));
					strcpy(pszFile, str);
				}
				else
				{
					*pReturnError = PVRTStringFromFormattedStr("Texture name missing in [TEXTURES] on line %d: %s\n", m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
					goto fail_release_return;
				}
			}

			if(strcmp(pszKeyword, "FILE") == 0)
			{
				// --- Filter flags
				{
					unsigned int* pFlags[3] =
					{
						&uiMin,
						&uiMag,
						&uiMip,
					};

					if(!ParseTextureFlags(pszRemaining, pFlags, 3, c_ppszFilters, eFilter_Size, pReturnError, i))
						goto fail_release_return;
				}

				// --- Wrap flags
				{
					unsigned int* pFlags[3] =
					{
						&uiWrapS,
						&uiWrapT,
						&uiWrapR,
					};

					if(!ParseTextureFlags(pszRemaining, pFlags, 3, c_ppszWraps, eWrap_Size, pReturnError, i))
						goto fail_release_return;
				}
	
				SPVRTPFXParserTexture* pTex = new SPVRTPFXParserTexture();
				pTex->Name				= pszName;
				pTex->FileName			= pszFile;
				pTex->bRenderToTexture	= false;
				pTex->nMin				= uiMin;
				pTex->nMag				= uiMag;
				pTex->nMIP				= uiMip;
				pTex->nWrapS			= uiWrapS;
				pTex->nWrapT			= uiWrapT;
				pTex->nWrapR			= uiWrapR;
				pTex->uiWidth			= uiWidth;
				pTex->uiHeight			= uiHeight;
				pTex->uiFlags			= uiFlags;
				m_psTexture.Append(pTex);
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [TEXTURES] on line %d\n", str, m_psContext->pnFileLineNumber[i]);;
				goto fail_release_return;
			}
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr("Missing arguments in [TEXTURES] on line %d: %s\n", m_psContext->pnFileLineNumber[i],  m_psContext->ppszEffectFile[i]);
			goto fail_release_return;
		}
	}

	/*
		Should only reach here if there have been no issues
	*/
	bReturnVal = true;
	goto release_return;

fail_release_return:
	bReturnVal = false;
release_return:
	FREE(pszKeyword);
	FREE(pszName);
	FREE(pszFile);
	FREE(pszTemp);
	return bReturnVal;
}

/*!***************************************************************************
@Function		ParseTextureFlags
@Input			c_pszCursor
@Output			pFlagsOut
@Input			uiNumFlags
@Input			ppszFlagNames
@Input			uiNumFlagNames
@Input			pReturnError
@Input			iLineNum
@Return			bool	
@Description	Parses the texture flag sections.
*****************************************************************************/
bool CPVRTPFXParser::ParseTextureFlags(	const char* c_pszRemainingLine, unsigned int** ppFlagsOut, unsigned int uiNumFlags, const char** c_ppszFlagNames, unsigned int uiNumFlagNames, 
										CPVRTString * const pReturnError, int iLineNum)
{
	const unsigned int INVALID_TYPE = 0xAC1DBEEF;
	unsigned int uiIndex;
	const char* c_pszCursor;
	const char* c_pszResult;

	// --- Find the first flag
	uiIndex = 0;
	c_pszCursor = strstr(c_pszRemainingLine, c_ppszFlagNames[uiIndex++]);
	while(uiIndex < uiNumFlagNames)
	{
		c_pszResult = strstr(c_pszRemainingLine, c_ppszFlagNames[uiIndex++]);
		if(((c_pszResult < c_pszCursor) || !c_pszCursor) && c_pszResult)
			c_pszCursor = c_pszResult;
	}

	if(!c_pszCursor)
		return true;		// No error, but just return as no flags specified.

	// Quick error check - make sure that the first flag found is valid.
	if(c_pszCursor != c_pszRemainingLine)
	{
		if(*(c_pszCursor-1) == '-')		// Yeah this shouldn't be there. Must be invalid first tag.
		{
			char szBuffer[128];		// Find out the tag.
			memset(szBuffer, 0, sizeof(szBuffer));
			const char* pszStart = c_pszCursor-1;
			while(pszStart != c_pszRemainingLine && *pszStart != ' ')		pszStart--;
			pszStart++;	// Escape the space.
			unsigned int uiNumChars = (c_pszCursor-1) - pszStart;
			strncpy(szBuffer, pszStart, uiNumChars);

			*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [TEXTURES] on line %d: %s\n", szBuffer, m_psContext->pnFileLineNumber[iLineNum], m_psContext->ppszEffectFile[iLineNum]);
			return false;
		}
	}

	unsigned int uiFlagsFound = 0;
	unsigned int uiBufferIdx;
	char szBuffer[128];		// Buffer to hold the token

	while(*c_pszCursor != ' ' && *c_pszCursor != 0 && uiFlagsFound < uiNumFlags)
	{
		memset(szBuffer, 0, sizeof(szBuffer));		// Clear the buffer
		uiBufferIdx = 0;

		while(*c_pszCursor != '-' && *c_pszCursor != 0 && *c_pszCursor != ' ' && uiBufferIdx < 128)		// - = delim. token
			szBuffer[uiBufferIdx++] = *c_pszCursor++;

		// Check if the buffer content is a valid flag name.
		unsigned int Type = INVALID_TYPE;
		for(unsigned int uiIndex = 0; uiIndex < uiNumFlagNames; ++uiIndex)
		{
			if(strcmp(szBuffer, c_ppszFlagNames[uiIndex]) == 0)	
			{
				Type = uiIndex;			// Yup, it's valid. uiIndex here would translate to one of the enums that matches the string array of flag names passed in.
				break;
			}
		}

		// Tell the user it's invalid.
		if(Type == INVALID_TYPE)
		{
			*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [TEXTURES] on line %d: %s\n", szBuffer, m_psContext->pnFileLineNumber[iLineNum], m_psContext->ppszEffectFile[iLineNum]);
			return false;
		}

		// Set the flag to the enum type.
		*ppFlagsOut[uiFlagsFound++] = Type;

		if(*c_pszCursor == '-')	c_pszCursor++;
	}

	return true;
}

/*!***************************************************************************
 @Function			ParseShader
 @Input				nStartLine		start line number
 @Input				nEndLine		end line number
 @Output			pReturnError	error string
 @Output			shader			shader data object
 @Input				pszBlockName	name of block in PFX file
 @Return			bool			true if parse is successful
 @Description		Parses the VERTEXSHADER or FRAGMENTSHADER section of the
					PFX file.
*****************************************************************************/
bool CPVRTPFXParser::ParseShader(int nStartLine, int nEndLine, CPVRTString * const pReturnError, SPVRTPFXParserShader &shader, const char * const pszBlockName)
{
	bool glslcode=0, glslfile=0, bName=0;

	shader.pszName			= NULL;
	shader.bUseFileName		= false;
	shader.pszGLSLfile		= NULL;
	shader.pszGLSLcode		= NULL;
	shader.pszGLSLBinaryFile= NULL;
	shader.pbGLSLBinary		= NULL;
	shader.nFirstLineNumber	= 0;

	for(int i = nStartLine+1; i < nEndLine; i++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[i])
			continue;

		char *str = strtok (m_psContext->ppszEffectFile[i]," ");
		if(str != NULL)
		{
			// Check for [GLSL_CODE] tags first and remove those lines from loop.
			if(strcmp(str, "[GLSL_CODE]") == 0)
			{
				if(glslcode)
				{
					*pReturnError = PVRTStringFromFormattedStr("[GLSL_CODE] redefined in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}
				if(glslfile && shader.pbGLSLBinary==NULL )
				{
					*pReturnError = PVRTStringFromFormattedStr("[GLSL_CODE] not allowed with FILE in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				shader.nFirstLineNumber = m_psContext->pnFileLineNumber[i];

				// Skip the block-start
				i++;

				if(!ConcatenateLinesUntil(
					shader.pszGLSLcode,
					i,
					m_psContext->ppszEffectFile,
					m_psContext->nNumLines,
					"[/GLSL_CODE]"))
				{
					return false;
				}

				shader.bUseFileName = false;
				glslcode = 1;
			}
			else if(strcmp(str, "NAME") == 0)
			{
				if(bName)
				{
					*pReturnError = PVRTStringFromFormattedStr("NAME redefined in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				str = ReadEOLToken(NULL);

				if(str == NULL)
				{
					*pReturnError = PVRTStringFromFormattedStr("NAME missing value in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				shader.pszName = (char*)malloc((strlen(str)+1) * sizeof(char));
				strcpy(shader.pszName, str);
				bName = true;
			}
			else if(strcmp(str, "FILE") == 0)
			{
				if(glslfile)
				{
					*pReturnError = PVRTStringFromFormattedStr("FILE redefined in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}
				if(glslcode)
				{
					*pReturnError = PVRTStringFromFormattedStr("FILE not allowed with [GLSL_CODE] in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				str = ReadEOLToken(NULL);

				if(str == NULL)
				{
					*pReturnError = PVRTStringFromFormattedStr("FILE missing value in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				shader.pszGLSLfile = (char*)malloc((strlen(str)+1) * sizeof(char));
				strcpy(shader.pszGLSLfile, str);

				CPVRTResourceFile GLSLFile(str);

				if(!GLSLFile.IsOpen())
				{
					*pReturnError = PVRTStringFromFormattedStr("Error loading file '%s' in [%s] on line %d\n", str, pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}
				shader.pszGLSLcode = new char[GLSLFile.Size() + 1];
				memcpy(shader.pszGLSLcode, (const char*) GLSLFile.DataPtr(), GLSLFile.Size());
				shader.pszGLSLcode[GLSLFile.Size()] = '\0';

				shader.nFirstLineNumber = m_psContext->pnFileLineNumber[i];		// Mark position where GLSL file is defined.

				shader.bUseFileName = true;
				glslfile = 1;
			}
			else if(strcmp(str, "BINARYFILE") == 0)
			{
				str = ReadEOLToken(NULL);

				if(str == NULL)
				{
					*pReturnError = PVRTStringFromFormattedStr("BINARYFILE missing value in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}

				shader.pszGLSLBinaryFile = (char*)malloc((strlen(str)+1) * sizeof(char));
				strcpy(shader.pszGLSLBinaryFile, str);

				CPVRTResourceFile GLSLFile(str);

				if(!GLSLFile.IsOpen())
				{
					*pReturnError = PVRTStringFromFormattedStr("Error loading file '%s' in [%s] on line %d\n", str, pszBlockName, m_psContext->pnFileLineNumber[i]);
					return false;
				}
				shader.pbGLSLBinary = new char[GLSLFile.Size()];
				shader.nGLSLBinarySize = (unsigned int)GLSLFile.Size();
				memcpy(shader.pbGLSLBinary, GLSLFile.DataPtr(), GLSLFile.Size());

				shader.bUseFileName = true;
				glslfile = 1;
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [%s] on line %d\n", str, pszBlockName, m_psContext->pnFileLineNumber[i]);
				return false;
			}

			str = strtok (NULL, " ");
			if(str != NULL)
			{
				*pReturnError = PVRTStringFromFormattedStr("Unexpected data in [%s] on line %d: '%s'\n", pszBlockName, m_psContext->pnFileLineNumber[i], str);
				return false;
			}
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr("Missing arguments in [%s] on line %d: %s\n", pszBlockName, m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
			return false;
		}
	}

	if(!bName)
	{
		*pReturnError = PVRTStringFromFormattedStr("NAME not found in [%s] on line %d.\n", pszBlockName, m_psContext->pnFileLineNumber[nStartLine]);
		return false;
	}

	if(!glslfile && !glslcode)
	{
		*pReturnError = PVRTStringFromFormattedStr("No Shader File or Shader Code specified in [%s] on line %d\n", pszBlockName, m_psContext->pnFileLineNumber[nStartLine]);
		return false;
	}

	return true;
}

/*!***************************************************************************
 @Function			ParseSemantic
 @Output			semantic		semantic data object
 @Input				nStartLine		start line number
 @Output			pReturnError	error string
 @Return			bool			true if parse is successful
 @Description		Parses a semantic.
*****************************************************************************/
bool CPVRTPFXParser::ParseSemantic(SPVRTPFXParserSemantic &semantic, const int nStartLine, CPVRTString * const pReturnError)
{
	char *str;

	semantic.pszName = 0;
	semantic.pszValue = 0;
	semantic.sDefaultValue.eType = eDataTypeNone;
	semantic.nIdx = 0;

	str = strtok (NULL, " ");
	if(str == NULL)
	{
		*pReturnError = PVRTStringFromFormattedStr("UNIFORM missing name in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);
		return false;
	}
	semantic.pszName = (char*)malloc((strlen(str)+1) * sizeof(char));
	strcpy(semantic.pszName, str);

	str = strtok (NULL, " ");
	if(str == NULL)
	{
		*pReturnError = PVRTStringFromFormattedStr("UNIFORM missing value in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);

		FREE(semantic.pszName);
		return false;
	}

	/*
		If the final digits of the semantic are a number they are
		stripped off and used as the index, with the remainder
		used as the semantic.
	*/
	{
		size_t idx, len;
		len = strlen(str);

		idx = len;
		while(idx)
		{
			--idx;
			if(strcspn(&str[idx], "0123456789") != 0)
			{
				break;
			}
		}
		if(idx == 0)
		{
			*pReturnError = PVRTStringFromFormattedStr("Semantic contains only numbers in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);

			FREE(semantic.pszName);
			return false;
		}

		++idx;
		// Store the semantic index
		if(len == idx)
		{
			semantic.nIdx = 0;
		}
		else
		{
			semantic.nIdx = atoi(&str[idx]);
		}

		// Chop off the index from the string containing the semantic
		str[idx] = 0;
	}

	// Store a copy of the semantic name
	semantic.pszValue = (char*)malloc((strlen(str)+1) * sizeof(char));
	strcpy(semantic.pszValue, str);

	/*
		Optional default semantic value
	*/
	char pszString[2048];
	strcpy(pszString,"");
	str = strtok (NULL, " ");
	if(str != NULL)
	{
		// Get all ramainning arguments
		while(str != NULL)
		{
			strcat(pszString, str);
			strcat(pszString, " ");
			str = strtok (NULL, " ");
		}

		// default value
		int i;
		for(i = 0; i < eNumDefaultDataTypes; i++)
		{
			if(strncmp(pszString, c_psSemanticDefaultDataTypeInfo[i].pszName, strlen(c_psSemanticDefaultDataTypeInfo[i].pszName)) == 0)
			{
				if(!GetSemanticDataFromString(	&semantic.sDefaultValue,
												&pszString[strlen(c_psSemanticDefaultDataTypeInfo[i].pszName)],
												c_psSemanticDefaultDataTypeInfo[i].eType,
												pReturnError
												))
				{
					*pReturnError = PVRTStringFromFormattedStr(" on line %d.\n", m_psContext->pnFileLineNumber[nStartLine]);

					FREE(semantic.pszValue);
					FREE(semantic.pszName);
					return false;
				}

				semantic.sDefaultValue.eType = c_psSemanticDefaultDataTypeInfo[i].eType;
				break;
			}
		}

		// invalid data type
		if(i == eNumDefaultDataTypes)
		{
			*pReturnError = PVRTStringFromFormattedStr("'%s' unknown on line %d.\n", pszString, m_psContext->pnFileLineNumber[nStartLine]);

			FREE(semantic.pszValue);
			FREE(semantic.pszName);
			return false;
		}

	}

	return true;
}

/*!***************************************************************************
 @Function			ParseEffect
 @Output			effect			effect data object
 @Input				nStartLine		start line number
 @Input				nEndLine		end line number
 @Output			pReturnError	error string
 @Return			bool			true if parse is successful
 @Description		Parses the EFFECT section of the PFX file.
*****************************************************************************/
bool CPVRTPFXParser::ParseEffect(SPVRTPFXParserEffect &effect, const int nStartLine, const int nEndLine, CPVRTString * const pReturnError)
{
	bool bName = false;
	bool bVertShader = false;
	bool bFragShader = false;

	effect.pszName					= NULL;
	effect.pszAnnotation			= NULL;
	effect.pszVertexShaderName		= NULL;
	effect.pszFragmentShaderName	= NULL;

	effect.nMaxTextures				= 100;
	effect.nNumTextures				= 0;
	effect.psTextures				= new SPVRTPFXParserEffectTexture[effect.nMaxTextures];

	effect.nMaxUniforms				= 100;
	effect.nNumUniforms				= 0;
	effect.psUniform				= new SPVRTPFXParserSemantic[effect.nMaxUniforms];

	effect.nMaxAttributes			= 100;
	effect.nNumAttributes			= 0;
	effect.psAttribute				= new SPVRTPFXParserSemantic[effect.nMaxAttributes];

	for(int i = nStartLine+1; i < nEndLine; i++)
	{
		// Skip blank lines
		if(!*m_psContext->ppszEffectFile[i])
			continue;

		char *str = strtok (m_psContext->ppszEffectFile[i]," ");
		if(str != NULL)
		{
			if(strcmp(str, "[ANNOTATION]") == 0)
			{
				if(effect.pszAnnotation)
				{
					*pReturnError = PVRTStringFromFormattedStr("ANNOTATION redefined in [EFFECT] on line %d: \n", m_psContext->pnFileLineNumber[i]);
					return false;
				}

				i++;		// Skip the block-start
				if(!ConcatenateLinesUntil(
					effect.pszAnnotation,
					i,
					m_psContext->ppszEffectFile,
					m_psContext->nNumLines,
					"[/ANNOTATION]"))
				{
					return false;
				}
			}
			else if(strcmp(str, "VERTEXSHADER") == 0)
			{
				if(bVertShader)
				{
					*pReturnError = PVRTStringFromFormattedStr("VERTEXSHADER redefined in [EFFECT] on line %d: \n", m_psContext->pnFileLineNumber[i]);
					return false;
				}

				str = ReadEOLToken(NULL);

				if(str == NULL)
				{
					*pReturnError = PVRTStringFromFormattedStr("VERTEXSHADER missing value in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
					return false;
				}
				effect.pszVertexShaderName = (char*)malloc((strlen(str)+1) * sizeof(char));
				strcpy(effect.pszVertexShaderName, str);

				bVertShader = true;
			}
			else if(strcmp(str, "FRAGMENTSHADER") == 0)
			{
				if(bFragShader)
				{
					*pReturnError = PVRTStringFromFormattedStr("FRAGMENTSHADER redefined in [EFFECT] on line %d: \n", m_psContext->pnFileLineNumber[i]);
					return false;
				}

				str = ReadEOLToken(NULL);

				if(str == NULL)
				{
					*pReturnError = PVRTStringFromFormattedStr("FRAGMENTSHADER missing value in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
					return false;
				}
				effect.pszFragmentShaderName = (char*)malloc((strlen(str)+1) * sizeof(char));
				strcpy(effect.pszFragmentShaderName, str);

				bFragShader = true;
			}
			else if(strcmp(str, "TEXTURE") == 0)
			{
				if(effect.nNumTextures < effect.nMaxTextures)
				{
					// texture number
					str = strtok(NULL, " ");
					if(str != NULL)
						effect.psTextures[effect.nNumTextures].nNumber = atoi(str);
					else
					{
						*pReturnError = PVRTStringFromFormattedStr("TEXTURE missing value in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
						return false;
					}

					// texture name
					str = strtok(NULL, " ");
					if(str != NULL)
					{
						effect.psTextures[effect.nNumTextures].Name = str;
					}
					else
					{
						*pReturnError = PVRTStringFromFormattedStr("TEXTURE missing value in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
						return false;
					}

					++effect.nNumTextures;
				}
				else
				{
					*pReturnError = PVRTStringFromFormattedStr("Too many textures in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
					return false;
				}
			}
			else if(strcmp(str, "UNIFORM") == 0)
			{
				if(effect.nNumUniforms < effect.nMaxUniforms)
				{
					if(!ParseSemantic(effect.psUniform[effect.nNumUniforms], i, pReturnError))
						return false;
					effect.nNumUniforms++;
				}
				else
				{
					*pReturnError = PVRTStringFromFormattedStr("Too many uniforms in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
					return false;
				}
			}
			else if(strcmp(str, "ATTRIBUTE") == 0)
			{
				if(effect.nNumAttributes < effect.nMaxAttributes)
				{
					if(!ParseSemantic(effect.psAttribute[effect.nNumAttributes], i, pReturnError))
						return false;
					effect.nNumAttributes++;
				}
				else
				{
					*pReturnError = PVRTStringFromFormattedStr("Too many attributes in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[i]);
					return false;
				}
			}
			else if(strcmp(str, "NAME") == 0)
			{
				if(bName)
				{
					*pReturnError = PVRTStringFromFormattedStr("NAME redefined in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);
					return false;
				}

				str = strtok (NULL, " ");
				if(str == NULL)
				{
					*pReturnError = PVRTStringFromFormattedStr("NAME missing value in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);
					return false;
				}

				effect.pszName = (char *)malloc((strlen(str)+1) * sizeof(char));
				strcpy(effect.pszName, str);
				bName = true;
			}
			else if(strcmp(str, "TARGET") == 0)
			{
				unsigned int uiIndex = effect.asTargets.Append();

				// Target requires 2 components
				CPVRTString* pVals[] = { &effect.asTargets[uiIndex].bufferType, &effect.asTargets[uiIndex].targetName };

				for(unsigned int uiVal = 0; uiVal < 2; ++uiVal)
				{
					str = strtok (NULL, " ");
					if(str == NULL)
					{
						*pReturnError = PVRTStringFromFormattedStr("TARGET missing value(s) in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);
						return false;
					}
					
					*(pVals[uiVal]) = str;
				}
			}
			else
			{
				*pReturnError = PVRTStringFromFormattedStr("Unknown keyword '%s' in [EFFECT] on line %d\n", str, m_psContext->pnFileLineNumber[i]);
				return false;
			}
		}
		else
		{
			*pReturnError = PVRTStringFromFormattedStr( "Missing arguments in [EFFECT] on line %d: %s\n", m_psContext->pnFileLineNumber[i], m_psContext->ppszEffectFile[i]);
			return false;
		}
	}

	// Check that every TEXTURE has a matching UNIFORM
	for(unsigned int uiTex = 0; uiTex < effect.nNumTextures; ++uiTex)
	{
		unsigned int uiTexUnit		= effect.psTextures[uiTex].nNumber;
		const CPVRTString& texName  = effect.psTextures[uiTex].Name;
		// Find UNIFORM associated with the TexUnit (e.g TEXTURE0).
		bool bFound = false;
		for(unsigned int uiUniform = 0; uiUniform < effect.nNumUniforms; ++uiUniform)
		{
			const SPVRTPFXParserSemantic& Sem = effect.psUniform[uiUniform];
			if(strcmp(Sem.pszValue, "TEXTURE") == 0 && Sem.nIdx == uiTexUnit)
			{
				bFound = true;
				break;
			}
		}

		if(!bFound)
		{
			*pReturnError = PVRTStringFromFormattedStr("TEXTURE %s missing matching UNIFORM in [EFFECT] on line %d\n", texName.c_str(), m_psContext->pnFileLineNumber[nStartLine]);
			return false;
		}
	}


	if(!bName)
	{
		*pReturnError = PVRTStringFromFormattedStr("No 'NAME' found in [EFFECT] on line %d\n", m_psContext->pnFileLineNumber[nStartLine]);
		return false;
	}
	if(!bVertShader)
	{
		*pReturnError = PVRTStringFromFormattedStr("No 'VERTEXSHADER' defined in [EFFECT] starting on line %d: \n", m_psContext->pnFileLineNumber[nStartLine-1]);
		return false;
	}
	if(!bFragShader)
	{
		*pReturnError = PVRTStringFromFormattedStr("No 'FRAGMENTSHADER' defined in [EFFECT] starting on line %d: \n", m_psContext->pnFileLineNumber[nStartLine-1]);
		return false;
	}

	return true;
}

/*!***************************************************************************
 @Function			DetermineRenderPassDependencies
 @Return			True if dependency tree is valid. False if there are errors
					in the dependency tree (e.g. recursion)
 @Description		Looks through all of the effects in the .pfx and determines
					the order of render passes that have been declared with
					the RENDER tag (found in [TEXTURES]
*****************************************************************************/
bool CPVRTPFXParser::DetermineRenderPassDependencies(CPVRTString * const pReturnError)
{
	unsigned int	ui(0), uj(0), uk(0);

	if(m_nNumRenderPasses == 0)
		return true;

	// --- Add all render pass nodes to the skip graph.
	for(ui = 0; ui < m_nNumRenderPasses; ++ui)
	{
		SPVRTPFXRenderPass& Pass = m_psRenderPasses[ui];
		bool bFound = false;

		// Search all EFFECT blocks for matching TARGET. This is for post-processes behavior.
		for(unsigned int uiEffect = 0; uiEffect < m_psEffect.GetSize(); ++uiEffect)
		{
			SPVRTPFXParserEffect& Effect = m_psEffect[uiEffect];

			// Search all TARGETs in this effect
			for(unsigned int uiTargets = 0; uiTargets < Effect.asTargets.GetSize(); ++uiTargets)
			{
				const SPVRTTargetPair& Target = Effect.asTargets[uiTargets];
				if(Target.targetName == Pass.SemanticName)
				{
					// Match. This EFFECT block matches the pass name.
					Pass.pEffect = &Effect;
					bFound = true;

					// This is now a post-process pass. Set relevant values.
					Pass.eRenderPassType = ePOSTPROCESS_PASS;
					m_aszPostProcessNames.Append(Pass.SemanticName);

					// Check that the surface type and output match are relevant (i.e DEPTH != RGBA8888).
					if( (Target.bufferType.find_first_of("DEPTH") != CPVRTString::npos && !(Pass.uiFormatFlags & PVRPFXTEX_DEPTH))
					||	(Target.bufferType.find_first_of("COLOR") != CPVRTString::npos && !(Pass.uiFormatFlags & PVRPFXTEX_COLOUR)) )
					{
						*pReturnError = PVRTStringFromFormattedStr("Surface type mismatch in [EFFECT]. \"%s\" has different type than \"%s\"\n", Target.targetName.c_str(), Pass.SemanticName.c_str());
						return false;
					}
					
					break;
				}
			}

			if(bFound)
				break;
		}

		// Add a pointer to the post process
		m_renderPassSkipGraph.AddNode(&Pass);
	}


	// --- Loop through all created render passes in the skip graph and determine their dependencies
	for(ui = 0; ui < m_renderPassSkipGraph.GetNumNodes(); ++ui)
	{
		//	Loop through all other nodes in the skip graph 
		SPVRTPFXRenderPass* pPass			= m_renderPassSkipGraph[ui];
		SPVRTPFXRenderPass* pTestPass       = NULL;

		for(uj = 0; uj < m_nNumRenderPasses; ++uj)
		{
			pTestPass = m_renderPassSkipGraph[uj];
				
			// No self compare
			if(pPass == pTestPass)
				continue;

			// No effect associated.
			if(!pPass->pEffect)			
				continue;

			// Is the node a render pass I rely on?
			for(uk = 0; uk < pPass->pEffect->nNumTextures; ++uk)
			{
				/*
					If the texture names match, add a new node
				*/
				if(pTestPass->pTexture->Name == pPass->pEffect->psTextures[uk].Name)
				{
					m_renderPassSkipGraph.AddNodeDependency(pPass, pTestPass);
					break;
				}
			}
		}
	}
	
	return true;
}

/*!***************************************************************************
@Function		FindTextureIndex
@Input			TextureName
@Return			unsigned int	Index in to the effect.Texture array.
@Description	Returns the index in to the texture array within the effect 
				block where the given texture resides.
*****************************************************************************/
unsigned int CPVRTPFXParser::FindTextureIndex( const CPVRTString& TextureName, unsigned int uiEffect ) const
{
	// TODO: Remove string compare. Change to hash check.
	for(unsigned int uiIndex = 0; uiIndex < m_psEffect[uiEffect].nNumTextures; ++uiIndex)
	{
		SPVRTPFXParserEffectTexture* pTex = &m_psEffect[uiEffect].psTextures[uiIndex];
		if(pTex->Name == TextureName)
		{
			return uiIndex;
		}
	}

	return 0xFFFFFFFF;
}

/*!***************************************************************************
@Function		GetNumberRenderPasses
@Return			unsigned int
@Description	Returns the number of render passes within this PFX.
*****************************************************************************/
unsigned int CPVRTPFXParser::GetNumberRenderPasses() const
{
	return m_nNumRenderPasses;
}

/*!***************************************************************************
@Function		GetNumberRenderPasses
@Input			unsigned int		The render pass index.
@Return			SPVRTPFXRenderPass*
@Description	Returns the given render pass.
*****************************************************************************/
const SPVRTPFXRenderPass& CPVRTPFXParser::GetRenderPass( unsigned int uiIndex ) const
{
	_ASSERT(uiIndex >= 0 && uiIndex < GetNumberRenderPasses());
	return m_psRenderPasses[uiIndex];
}

/*!***************************************************************************
@Function		GetPFXFileName
@Return			const CPVRTString &	
@Description	Returns the PFX file name associated with this object.
*****************************************************************************/
const CPVRTString& CPVRTPFXParser::GetPFXFileName() const
{
	return m_szFileName;
}

/*!***************************************************************************
@Function		GetPostProcessNames
@Return			const CPVRTArray<CPVRTString>&	
@Description	Returns a list of prost process effect names.
*****************************************************************************/
const CPVRTArray<CPVRTString>& CPVRTPFXParser::GetPostProcessNames() const
{
	return m_aszPostProcessNames;
}

/*!***************************************************************************
@Function		GetNumberFragmentShaders
@Return			unsigned int	Number of fragment shaders.
@Description	Returns the number of fragment shaders referenced in the PFX.
*****************************************************************************/
unsigned int CPVRTPFXParser::GetNumberFragmentShaders() const
{
	return m_psFragmentShader.GetSize();
}


/*!***************************************************************************
@Function		GetFragmentShader
@Input			unsigned int		The index of this shader.
@Return			const SPVRTPFXParserShader&		The PFX fragment shader.
@Description	Returns a given fragment shader.
*****************************************************************************/
SPVRTPFXParserShader& CPVRTPFXParser::GetFragmentShader( unsigned int uiIndex )
{
	_ASSERT(uiIndex < GetNumberFragmentShaders());
	return m_psFragmentShader[uiIndex];
}

/*!***************************************************************************
@Function		GetNumberVertexShaders
@Return			unsigned int	Number of vertex shaders.
@Description	Returns the number of vertex shaders referenced in the PFX.
*****************************************************************************/
unsigned int CPVRTPFXParser::GetNumberVertexShaders() const
{
	return m_psVertexShader.GetSize();
}

/*!***************************************************************************
@Function		GetVertexShader
@Input			unsigned int		The index of this shader.
@Return			const SPVRTPFXParserShader&		The PFX vertex shader.
@Description	Returns a given vertex shader.
*****************************************************************************/
SPVRTPFXParserShader& CPVRTPFXParser::GetVertexShader( unsigned int uiIndex )
{
	_ASSERT(uiIndex < GetNumberVertexShaders());
	return m_psVertexShader[uiIndex];
}

/*!***************************************************************************
@Function		GetNumberEffects
@Return			unsigned int	Number of effects.
@Description	Returns the number of effects referenced in the PFX.
*****************************************************************************/
unsigned int CPVRTPFXParser::GetNumberEffects() const
{
	return m_psEffect.GetSize();
}

/*!***************************************************************************
@Function		GetEffect
@Input			unsigned int		The index of this effect.
@Return			const SPVRTPFXParserEffect&		The PFX effect.
@Description	Returns a given effect.
*****************************************************************************/
const SPVRTPFXParserEffect& CPVRTPFXParser::GetEffect( unsigned int uiIndex ) const
{
	_ASSERT(uiIndex < GetNumberEffects());
	return m_psEffect[uiIndex];
}

/*!***************************************************************************
@Function		GetNumberTextures
@Return			unsigned int	Number of effects.
@Description	Returns the number of textures referenced in the PFX.
*****************************************************************************/
unsigned int CPVRTPFXParser::GetNumberTextures() const
{
	return m_psTexture.GetSize();
}

/*!***************************************************************************
@Function		GetTexture
@Input			unsigned int		The index of this texture
@Return			const SPVRTPFXParserEffect&		The PFX texture.
@Description	Returns a given texture.
*****************************************************************************/
const SPVRTPFXParserTexture* CPVRTPFXParser::GetTexture( unsigned int uiIndex ) const
{
	_ASSERT(uiIndex < GetNumberTextures());
	return m_psTexture[uiIndex];
}



/*!***************************************************************************
@Function		PVRTPFXCreateStringCopy
@Return			void
@Description	Safely copies a C string.
*****************************************************************************/
void PVRTPFXCreateStringCopy(char** ppDst, const char* pSrc)
{
	if(pSrc)
	{
		FREE(*ppDst);
		*ppDst = (char*)malloc((strlen(pSrc)+1) * sizeof(char));
		strcpy(*ppDst, pSrc);
	}
}

/*****************************************************************************
 End of file (PVRTPFXParser.cpp)
*****************************************************************************/

