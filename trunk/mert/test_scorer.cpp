#include <iostream>
#include <vector>

#include "ScoreData.h"
#include "Scorer.h"

using namespace std;

int main(int argc, char** argv) {
	cout << "Testing the scorer" << endl;	
	//BleuScorer bs("test-scorer-data/cppstats.feats.opt");;
	vector<string> references;
	references.push_back("test_scorer_data/reference.txt");
	//bs.prepare(references, "test-scorer-data/nbest.out");
	Scorer* scorer = new BleuScorer();;
	scorer->setReferenceFiles(references);
	ScoreData sd(*scorer);
	sd.loadnbest("test_scorer_data/nbest.out");
	//sd.savetxt();

	//calculate two   bleu scores, nbest and a diff
	scorer->setScoreData(&sd);
	candidates_t candidates(sd.size());;
	for (size_t i  = 0; i < sd.size(); ++i) {
        sd.get(i,0).savetxt("/dev/stdout");
	}

    diffs_t diffs;
    diff_t diff;
    diff.push_back(make_pair(1,2));
    diff.push_back(make_pair(7,8));
    diffs.push_back(diff);
    
    scores_t scores;
    scorer->score(candidates,diffs,scores);

	cout << "Bleus: " << scores[0] << " " << scores[1] <<  endl;

    //try the per
    scorer = new PerScorer();
    ScoreData psd(*scorer);
    scorer->setReferenceFiles(references);

	psd.loadnbest("test_scorer_data/nbest.out");
	//sd.savetxt();

	scorer->setScoreData(&psd);
	for (size_t i  = 0; i < psd.size(); ++i) {
        psd.get(i,0).savetxt("/dev/stdout");
	}

    cout << "PER: " << scorer->score(candidates) << endl;
 
}
