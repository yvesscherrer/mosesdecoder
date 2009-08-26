/*
 *  PhraseDictionaryBerkeleyDbChart.cpp
 *  moses
 *
 *  Created by Hieu Hoang on 31/07/2009.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */
#include "PhraseDictionaryBerkeleyDb.h"
#include "StaticData.h"
#include "DotChartBerkeleyDb.h"
#include "CellCollection.h"
#include "../../BerkeleyPt/src/TargetPhraseCollection.h"

using namespace std;

namespace Moses
{	
	
class TempStore
	{
	public:
		const WordConsumed *m_wordConsumed;
		const MosesBerkeleyPt::TargetPhraseCollection *m_tpColl;
		float m_sourceCount, m_entropy;
		
		TempStore(const WordConsumed *wordConsumed, 
							const MosesBerkeleyPt::TargetPhraseCollection *tpColl
							,float sourceCount, float entropy)
		:m_wordConsumed(wordConsumed)
		,m_tpColl(tpColl)
		,m_sourceCount(sourceCount)
		,m_entropy(entropy)
		{}
	};

	
const ChartRuleCollection *PhraseDictionaryBerkeleyDb::GetChartRuleCollection(
																																					InputType const& src
																																					,WordsRange const& range
																																					,bool adhereTableLimit
																																					,const CellCollection &cellColl) const
{
	const StaticData &staticData = StaticData::Instance();
	float weightWP = staticData.GetWeightWordPenalty();
	const LMList &lmList = staticData.GetAllLM();

	// source phrase
	Phrase *cachedSource = new Phrase(src.GetSubString(range));
	m_sourcePhrase.push_back(cachedSource);

	ChartRuleCollection *ret = new ChartRuleCollection();
	m_chartTargetPhraseColl.push_back(ret);

	size_t relEndPos = range.GetEndPos() - range.GetStartPos();
	size_t absEndPos = range.GetEndPos();

	// MAIN LOOP. create list of nodes of target phrases
	ProcessedRuleStackBerkeleyDb &runningNodes = *m_runningNodesVec[range.GetStartPos()];

	const ProcessedRuleStackBerkeleyDb::SavedNodeColl &savedNodeColl = runningNodes.GetSavedNodeColl();
	for (size_t ind = 0; ind < savedNodeColl.size(); ++ind)
	{
		const SavedNodeBerkeleyDb &savedNode = *savedNodeColl[ind];
		const ProcessedRuleBerkeleyDb &prevProcessedRule = savedNode.GetProcessedRule();
		const MosesBerkeleyPt::SourcePhraseNode &prevNode = prevProcessedRule.GetLastNode();
		const WordConsumed *prevWordConsumed = prevProcessedRule.GetLastWordConsumed();
		size_t startPos = (prevWordConsumed == NULL) ? range.GetStartPos() : prevWordConsumed->GetWordsRange().GetEndPos() + 1;

		// search for terminal symbol
		if (startPos == absEndPos)
		{
			const Word &sourceWord = src.GetWord(absEndPos);
			MosesBerkeleyPt::Word *sourceWordBerkeleyDb = m_dbWrapper.ConvertFromMoses(Input, m_inputFactorsVec, sourceWord);
	
			if (sourceWordBerkeleyDb != NULL)
			{
				const MosesBerkeleyPt::SourcePhraseNode *node = m_dbWrapper.GetChild(prevNode, *sourceWordBerkeleyDb);
				if (node != NULL)
				{
					// TODO figure out why source word is needed from node, not from sentence
					// prob to do with factors or non-term
					//const Word &sourceWord = node->GetSourceWord();
					WordConsumed *newWordConsumed = new WordConsumed(absEndPos, absEndPos
																														, sourceWord
																														, prevWordConsumed);
					ProcessedRuleBerkeleyDb *processedRule = new ProcessedRuleBerkeleyDb(*node, newWordConsumed);
					runningNodes.Add(relEndPos+1, processedRule);
					
					// cache for cleanup
					m_sourcePhraseNode.push_back(node);
				}

				delete sourceWordBerkeleyDb;
			}
		}

		// search for non-terminals
		size_t endPos, stackInd;
		if (startPos > absEndPos)
			continue;
		else if (startPos == range.GetStartPos() && range.GetEndPos() > range.GetStartPos())
		{ // start.
			endPos = absEndPos - 1;
			stackInd = relEndPos;
		}
		else
		{
			endPos = absEndPos;
			stackInd = relEndPos + 1;
		}

		// get target headwords in this span from chart
		const vector<Word> &headWords = cellColl.GetHeadwords(WordsRange(startPos, endPos));

		// go through each SOURCE lhs
		const Sentence &sentence = static_cast<const Sentence&>(src);
		const LabelList &labelList = sentence.GetLabelList(startPos, endPos);
		LabelList::const_iterator iterLabelList;
		for (iterLabelList = labelList.begin(); iterLabelList != labelList.end(); ++iterLabelList)
		{
			const Word &sourceLabel = *iterLabelList;
			MosesBerkeleyPt::Word *sourceLHSBerkeleyDb = m_dbWrapper.ConvertFromMoses(Input, m_inputFactorsVec, sourceLabel);
		
			if (sourceLHSBerkeleyDb == NULL)
			{
				delete sourceLHSBerkeleyDb;
				continue; // vocab not in pt. node definately won't be in there
			}

			const MosesBerkeleyPt::SourcePhraseNode *sourceNode = m_dbWrapper.GetChild(prevNode, *sourceLHSBerkeleyDb);
			delete sourceLHSBerkeleyDb;

			if (sourceNode == NULL)
				continue; // didn't find source node

			// go through each TARGET lhs
			vector<Word>::const_iterator iterHeadWords;
			for (iterHeadWords = headWords.begin(); iterHeadWords != headWords.end(); ++iterHeadWords)
			{
				const Word &headWord = *iterHeadWords;
				MosesBerkeleyPt::Word *headWordBerkeleyDb = m_dbWrapper.ConvertFromMoses(Output, m_outputFactorsVec, headWord);

				if (headWordBerkeleyDb == NULL)
					continue;

				const MosesBerkeleyPt::SourcePhraseNode *node = m_dbWrapper.GetChild(*sourceNode, *headWordBerkeleyDb);
				delete headWordBerkeleyDb;

				if (node == NULL)
					continue;

				// found matching entry
				//const Word &sourceWord = node->GetSourceWord();
				WordConsumed *newWordConsumed = new WordConsumed(startPos, endPos
																													, headWord
																													, prevWordConsumed);

				ProcessedRuleBerkeleyDb *processedRule = new ProcessedRuleBerkeleyDb(*node, newWordConsumed);
				runningNodes.Add(stackInd, processedRule);

				m_sourcePhraseNode.push_back(node);

			} // for (iterHeadWords

			delete sourceNode;

		} // for (iterLabelList
	} // for (size_t ind = 0; ind < savedNodeColl.size(); ++ind)


	// return list of target phrases
	float minEntropy = 99999;
	float minEntropyCount = 0;
	
	const ProcessedRuleCollBerkeleyDb &nodes = runningNodes.Get(relEndPos + 1);
	
	vector<TempStore> listTpColl;
	
	size_t rulesLimit = StaticData::Instance().GetRuleLimit();
	ProcessedRuleCollBerkeleyDb::const_iterator iterNode;
	for (iterNode = nodes.begin(); iterNode != nodes.end(); ++iterNode)
	{
		const ProcessedRuleBerkeleyDb &processedRule = **iterNode;
		const MosesBerkeleyPt::SourcePhraseNode &node = processedRule.GetLastNode();
		const WordConsumed *wordConsumed = processedRule.GetLastWordConsumed();
		assert(wordConsumed);

		float sourceCount, entropy;
		
		const MosesBerkeleyPt::TargetPhraseCollection *tpcollBerkeleyDb = m_dbWrapper.GetTargetPhraseCollection(node, sourceCount, entropy);

		if (sourceCount > 10)
		{
			minEntropyCount = (entropy<minEntropy) ? sourceCount : minEntropyCount;
			minEntropy = (entropy<minEntropy) ? entropy : minEntropy;
		}
		
		listTpColl.push_back(TempStore(wordConsumed, tpcollBerkeleyDb, sourceCount, entropy));		
	}
	
	minEntropy *= 2;
	
	cerr << minEntropyCount << "|" << minEntropy << " ";
	
	vector<TempStore>::const_iterator iterListTpColl;
	for (iterListTpColl = listTpColl.begin(); iterListTpColl != listTpColl.end(); ++iterListTpColl)
	{
		const WordConsumed *wordConsumed = iterListTpColl->m_wordConsumed;
		const MosesBerkeleyPt::TargetPhraseCollection *tpcollBerkeleyDb = iterListTpColl->m_tpColl;
		float sourceCount = iterListTpColl->m_sourceCount;
		float entropy = iterListTpColl->m_entropy;
		
		if (true)
		//if (minEntropy > 99999 || (sourceCount > 10 && entropy <= minEntropy))
		{
			TargetPhraseCollection *targetPhraseCollection = m_dbWrapper.ConvertToMoses(
																																								*tpcollBerkeleyDb
																																								,m_inputFactorsVec
																																								,m_outputFactorsVec
																																								,*this
																																								,m_weight
																																								,weightWP
																																								,lmList
																																								,*cachedSource);
			assert(targetPhraseCollection);
			//cerr << *targetPhraseCollection << endl;
			ret->Add(*targetPhraseCollection, *wordConsumed, adhereTableLimit, rulesLimit);
			m_cache.push_back(targetPhraseCollection);
		}
		else
		{
			cerr << sourceCount << "|" << entropy << " ";
		}
		
		delete tpcollBerkeleyDb;
	}
	
	cerr << endl;
	
	ret->CreateChartRules(rulesLimit);
	
	return ret;
}
	
}; // namespace

