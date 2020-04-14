//  MABE is a product of The Hintze Lab @ MSU
//     for general research information:
//         hintzelab.msu.edu
//     for MABE documentation:
//         github.com/Hintzelab/MABE/wiki
//
//  Copyright (c) 2015 Michigan State University. All rights reserved.
//     to view the full license, visit:
//         github.com/Hintzelab/MABE/wiki/License

// Evaluates agents in an NK landscape
// Landscape can be changing periodically (treadmilling) or static

// Note that brain outputs must be 0 or 1; there is a conversion from negative doubles to positive doubles in the landscape
// If brain outputs are only positive, your organisms will have a bad time

#include "NKWorld.h"
#include "../../Utilities/Random.h"
#include <vector>
#include <map>
#include <iostream>
#include <fstream>

#define PI 3.14159265

template <typename T>
class Matrix2D{
private:
    std::vector<T> vec;
    size_t num_rows;
    size_t num_cols;
public:
    Matrix2D(size_t r, size_t c, T val){
        num_rows = r;
        num_cols = c;
        vec.resize(num_rows * num_cols, val);
    }
    inline T Get(size_t row, size_t col){
        if(row >= num_rows || col >= num_cols){
            std::cerr << "Error! Matrix index out of bounds!" << std::endl;
            std::cerr << "Requested: (" << row << ", " << col << ")" << std::endl;
            std::cerr << "Matrix size: (" << num_rows << ", " << num_cols << ")" << std::endl;
        }
        return vec[row * num_cols + col];
    }
    void Set(int row, int col, T val){
        vec[row * num_cols + col] = val;
    }
    void Print(std::ostream& os = std::cout){
        for(size_t row = 0; row < num_rows; ++row){
            for(size_t col = 0; col < num_cols; ++col){
                std::cout << Get(row, col) << " ";
            }
            std::cout << std::endl;
        }
    }
    void PrintWithLabels(const std::vector<T>& row_labels, const std::vector<T>& col_labels,
            std::ostream& os =  std::cout){
        std::cout << "xxx xxx xxx ";
        for(size_t col = 0; col < num_cols - 2; ++col){
            if(col_labels[col] < 10) std::cout << "  " << col_labels[col] << " ";
            else if(col_labels[col] < 100) std::cout << " " << col_labels[col] << " ";
            else std::cout << col_labels[col] << " ";
        }
        std::cout << std::endl;
        for(size_t row = 0; row < num_rows; ++row){
            if(row <= 1) std::cout << "xxx ";
            else{
                if(row_labels[row - 2] < 10) std::cout << "  " << row_labels[row - 2] << " ";
                else if(row_labels[row - 2] < 100) std::cout << " " << row_labels[row - 2] << " ";
                else std::cout <<  row_labels[row - 2] << " ";
            }
            for(size_t col = 0; col < num_cols; ++col){
                if(Get(row, col) < 10) std::cout << "  " << Get(row, col) << " ";
                else if(Get(row, col) < 100) std::cout << " " << Get(row, col) << " ";
                else std::cout << Get(row, col) << " ";
            }
            std::cout << std::endl;
        }
    }
};


enum EditDistanceMetric{
    kLevenshtein = 0,
    kDamerau_Levenshtein = 1
};

// Levenshtein 
// Implemented from psuedocode at https://en.wikipedia.org/wiki/Levenshtein_distance
double EditDistance_L(const std::vector<size_t>& vec_a, const std::vector<size_t>& vec_b){
    Matrix2D<size_t> matrix(vec_a.size() + 1, vec_b.size() + 1, 0);
    for(size_t i = 0; i <= vec_a.size(); ++i) matrix.Set(i, 0, i);
    for(size_t j = 0; j <= vec_b.size(); ++j) matrix.Set(0, j, j);
    size_t sub_cost = 0;
    for(size_t j = 1; j <= vec_b.size(); ++j){
        for(size_t i = 1; i <= vec_a.size(); ++i){
            sub_cost = (vec_a[i-1] == vec_b[j-1]) ? 0 : 1;
            matrix.Set(i, j, std::min({
                matrix.Get(i, j - 1) + 1,
                matrix.Get(i - 1, j) + 1,
                matrix.Get(i - 1, j - 1) + sub_cost }));
        }
    }
    //matrix.Print();
    return matrix.Get(vec_a.size() - 1, vec_b.size() - 1);
}

// Damerau-Levenshtein
// From psuedocode at https://en.wikipedia.org/wiki/Damerau%E2%80%93Levenshtein_distance 
// Also useful for understanding: https://www.lemoda.net/text-fuzzy/damerau-levenshtein/index.html
double EditDistance_DL(const std::vector<size_t>& vec_a, const std::vector<size_t>& vec_b){
    std::vector<size_t> row_labels = {0, 0};
    for(size_t x : vec_a) row_labels.push_back(x);
    std::vector<size_t> col_labels = {0};
    for(size_t x : vec_b) col_labels.push_back(x);
    
    size_t max_a = *std::max_element(vec_a.begin(), vec_a.end());
    size_t max_b = *std::max_element(vec_b.begin(), vec_b.end());
    std::vector<size_t> da(std::max(max_a, max_b) + 1, 1);
    Matrix2D<size_t> matrix(vec_a.size() + 2, vec_b.size() + 2, 0);
    size_t max_dist = vec_a.size() + vec_b.size();    
    matrix.Set(0, 0, max_dist);
    for(size_t i = 0; i <= vec_a.size() + 1; ++i){
        matrix.Set(i, 0, max_dist);
        matrix.Set(i, 1, i - 1);
    }
    for(size_t j = 1; j <= vec_b.size() + 1; ++j){
        matrix.Set(0, j, max_dist);
        matrix.Set(1, j, j - 1);
    }


    for(size_t i = 2; i <= vec_a.size() + 1; ++i){
        size_t db = 1;
        for(size_t j = 2; j <= vec_b.size() + 1; ++j){
            size_t k = da[vec_b[j - 2]]; // Last row we saw current symbol in vec_b
            size_t l = db;
            size_t cost = 0;
            if(vec_a[i - 2] == vec_b[j - 2]){
                cost = 0;
                db = j;
            }
            else
                cost = 1;
            matrix.Set(i, j, std::min({
                matrix.Get(i - 1, j - 1) + cost,  // Substitution
                matrix.Get(i, j - 1) + 1, // Insertion
                matrix.Get(i - 1, j) + 1, // Deletion
                matrix.Get(k - 1, l - 1) + (i - k - 1) + 1 + (j - l - 1) // Transposition
            }));
        }
        da[vec_a[i - 2]] = i; // The last row we current symbol win vec_a is this one!
    }
    //matrix.PrintWithLabels(vec_a, vec_b);
    return matrix.Get(vec_a.size() + 1, vec_b.size() + 1);
}

// General edit distance function that will direct you to the specified metric
double EditDistance(const std::vector<size_t>& vec_a, const std::vector<size_t>& vec_b, 
        EditDistanceMetric metric){
    switch(metric){
        case kLevenshtein:
            return EditDistance_L(vec_a, vec_b);    
        break;
        case kDamerau_Levenshtein:
            return EditDistance_DL(vec_a, vec_b);    
        break;
        default:
            std::cerr << "Error! Unknown edit distance metric!" << std::endl;
            exit(-1);
            return 0;
        break;
    }
    return 0;
}

std::shared_ptr<ParameterLink<int>> NKWorld::nPL =
Parameters::register_parameter("WORLD_NK-n", 4,
        "number of outputs (e.g. traits, loci)");
std::shared_ptr<ParameterLink<int>> NKWorld::kPL =
Parameters::register_parameter("WORLD_NK-k", 2,
        "Number of sites each site interacts with");
std::shared_ptr<ParameterLink<bool>> NKWorld::treadmillPL =
Parameters::register_parameter("WORLD_NK-treadmill", false,
        "whether landscape should treadmill over time. "
        "0 = static landscape, 1 = treadmilling landscape");
std::shared_ptr<ParameterLink<bool>> NKWorld::readNKTablePL =
Parameters::register_parameter("WORLD_NK-readNKTable", false,
        "If true, reads the NK table from the file "
        "specified by inputNKTableFilename "
        "0 = random, 1 = load from file");
std::shared_ptr<ParameterLink<std::string>> NKWorld::inputNKTableFilenamePL =
Parameters::register_parameter("WORLD_NK-inputNKTableFilename", (std::string) "./table_csv",
        "If readNKTable is 1, which file should we use "
        "to load the table?");
std::shared_ptr<ParameterLink<bool>> NKWorld::writeNKTablePL =
Parameters::register_parameter("WORLD_NK-writeNKTable", true,
        "do you want the NK table to be output for each replicate? "
        "0 = no output, 1 = please output");
std::shared_ptr<ParameterLink<double>> NKWorld::velocityPL =
Parameters::register_parameter("WORLD_NK-velocity", 0.01,
        "If treadmilling, how fast should it treadmill? "
        "Smaller values = slower treadmill");
std::shared_ptr<ParameterLink<int>> NKWorld::evaluationsPerGenerationPL =
Parameters::register_parameter("WORLD_NK-evaluationsPerGeneration", 1,
        "Number of times to test each Genome per "
        "generation (useful with non-deterministic "
        "brains)");
std::shared_ptr<ParameterLink<std::string>> NKWorld::groupNamePL =
Parameters::register_parameter("WORLD_NK_NAMES-groupNameSpace",
        (std::string) "root::",
        "namespace of group to be evaluated");
std::shared_ptr<ParameterLink<bool>> NKWorld::outputMutantFitnessPL =
Parameters::register_parameter("WORLD_NK_OUTPUT-outputMutantFitness", false,
        "If true, output the fitness of one-bit mutants");
std::shared_ptr<ParameterLink<std::string>> NKWorld::outputMutantFitnessFilenamePL =
Parameters::register_parameter("WORLD_NK_OUTPUT-outputMutantFitnessFilename", 
        (std::string)"mutant_data.csv",
        "If we output mutant fitness, where to save it?");
std::shared_ptr<ParameterLink<int>> NKWorld::outputMutantFitnessIntervalPL =
Parameters::register_parameter("WORLD_NK_OUTPUT-outputMutantFitnessInterval", 
        100,
        "If we output mutant fitness, how often do we do so?");
std::shared_ptr<ParameterLink<int>> NKWorld::outputEditDistanceMetricPL =
Parameters::register_parameter("WORLD_NK_OUTPUT-outputEditDistanceMetric", 
        0,
        "Which edit distance to use. 0 for Levenshtein, 1 for Damerau-Levenshtein");

std::shared_ptr<ParameterLink<std::string>> NKWorld::brainNamePL =
Parameters::register_parameter(
        "WORLD_NK_NAMES-brainNameSpace", (std::string) "root::",
        "namespace for parameters used to define brain");

NKWorld::NKWorld(std::shared_ptr<ParametersTable> PT_)
    : AbstractWorld(PT_) {

    // localize N & K parameters
    N = nPL->get(PT);
    K = kPL->get(PT);

    output_mutant_fitness = outputMutantFitnessPL->get(PT);
    output_mutant_fitness_filename = outputMutantFitnessFilenamePL->get(PT);
    output_mutant_fitness_interval = outputMutantFitnessIntervalPL->get(PT);
    edit_distance_metric = outputEditDistanceMetricPL->get(PT);
    
    // generate NK lookup table
    // dimensions: N x 2^K
    // each value is a randomly generated pair of doubles, each in [0.0,1.0]
    // represents weighting on the fitness fcn for each k-tuple in the brain output state
    NKTable.clear();
    NKTable.resize(N);
    if(readNKTablePL->get(PT)){
        std::cout << "Attempting to read NK table from file: " << inputNKTableFilenamePL->get(PT) 
                  << std::endl;
        std::ifstream tableFP;
        tableFP.open(inputNKTableFilenamePL->get(PT), std::ios::in);
        if(!tableFP.is_open()){
            std::cerr << "ERROR! Unable to open file: " 
                      << inputNKTableFilenamePL->get(PT) << std::endl;
            exit(-1);
        }
        double cur_val = 0;
        std::cout << "NK table:" << std::endl;
        for(size_t n = 0; n < N; ++n){
            NKTable[n].resize(1<<K);
            for(int k=0;k<(1<<K);k++){
                tableFP >> cur_val;
                NKTable[n][k]= std::pair<double,double>(cur_val, 0);
                if(k != 0) std::cout << " ";
                std::cout << cur_val;
            }
            std::cout << std::endl;
        } 
    }
    else{
        for(int n=0;n<N;n++){
            NKTable[n].resize(1<<K);
            for(int k=0;k<(1<<K);k++){
                NKTable[n][k]= std::pair<double,double>(Random::getDouble(0.0,1.0),Random::getDouble(0.0,1.0));
            }
        }  
    }

    if (writeNKTablePL->get(PT)) {
        std::ofstream NKTable_csv;
        NKTable_csv.open("NKTable.csv");
        for(int k=0;k<(1<<K);k++){
            for(int n=0;n<N;n++){
                NKTable_csv << NKTable[n][k].first;
                // we don't want commas on the last one
                if (n < N-1) {
                    NKTable_csv << ",";
                } else {
                    NKTable_csv << "\n";
                }
            }
        }
        NKTable_csv.close();
    }

    // columns to be added to ave file
    popFileColumns.clear();
    popFileColumns.push_back("score");
    popFileColumns.push_back("score_VAR"); // specifies to also record the
    // variance (performed automatically
    // because _VAR)
    
}

// create angular sin function for more even fitness distribution
double NKWorld::triangleSin(double x) {
    double Y = 0.0;
    for (int i = 0; i<N; i++) {
        double n = (2*i) + 1;
        Y += pow(-1, (double)i)*pow(n, -2.0)*sin(n*x);
    }
    return (0.25*PI)*Y;
}


void NKWorld::evaluateSolo(std::shared_ptr<Organism> org, int analyze,
        int visualize, int debug) {
    auto brain = org->brains[brainNamePL->get(PT)];
    double t = Global::update*(velocityPL->get(PT));
    for (int r = 0; r < evaluationsPerGenerationPL->get(PT); r++) {

        brain->resetBrain();
        brain->update();

        // fitness function
        double W = 0.0;
        for (int n=0;n<N;n++) {
            //std::cout << "(" << n << ") ";
            int val = 0;
            for (int k=0; k<K; k++) {
                //std::cout <<  (brain->readOutput((n+k)%N) > 0.0);
                val = (val<<1) + (brain->readOutput((n+k)%N) > 0.0); // convert k adjacent sites to integer for indexing NK table
            }
            if (treadmillPL->get(PT)) {
                // formula for localValue generated by Arend Hintze
                double alpha = NKTable[n][val].first;   
                double beta = NKTable[n][val].second;
                double localValue = (1.0 + triangleSin((t*(beta+0.5))+(alpha*PI*2.0)))/2.0;
                W += localValue;
            } else {
                W += NKTable[n][val].first;
            }
            //std::cout << " " << NKTable[n][val].first << " ";
        }
        //std::cout << std::endl;
        double score = W/(double)N;

        org->dataMap.append("score", score);
        if (visualize) {
            std::string filename = "postscore_" + Global::initPopPL->get(PT) + ".csv";
            std::ofstream fileout;
            fileout.open(filename,std::ios::app);
            fileout << org->ID << "," << score << "\n";
            fileout.close();
        }
    }
}

void NKWorld::recordMutantFitness(std::map<std::string, std::shared_ptr<Group>> &groups){
        std::cout << "Recording edit distance..." << std::endl;
        output_string_stream.str("");
        int popSize = groups[groupNamePL->get(PT)]->population.size();
        rank_vec_original.resize(popSize);  
        rank_vec_mutated.resize(popSize);
        std::iota(rank_vec_original.begin(), rank_vec_original.end(), 0);
        mutant_data_vec.resize(popSize);
        for (int i = 0; i < popSize; i++) {
            // Get organism, evaluate it, cache score
            auto org_copy = groups[groupNamePL->get(PT)]->population[i]->makeCopy(); 
            evaluateSolo(org_copy, 0, 0, 0); 
            mutant_data_vec[i].score_original = org_copy->dataMap.getAverage("score");
        }
        // Sort orgs based on original fitness. Assign ranks 0 through N-1
        std::stable_sort(mutant_data_vec.begin(), mutant_data_vec.end(), 
            [](const NKOrgData& a, const NKOrgData& b){
           return a.score_original < b.score_original; 
        });
        for (int i = 0; i < popSize; i++) {
            mutant_data_vec[i].rank = i;
        }
        for(size_t bit_idx = 0; bit_idx < N; ++bit_idx){
            for (int i = 0; i < popSize; i++) {
                // Get organism, evaluate it, cache score
                auto org_copy = groups[groupNamePL->get(PT)]->population[i]->makeCopy(); 
                // Mutate organism
                std::shared_ptr<AbstractGenome> g = org_copy->genomes["root::"];
                std::shared_ptr<CircularGenome<bool>::Handler> genome_original = 
                    std::dynamic_pointer_cast<CircularGenome<bool>::Handler>(
                        g->newHandler(g));
                CircularGenome<bool> tmp_mutant_genome(2, 200, PT);
                std::shared_ptr<AbstractGenome> tmp_mutant_genome_ptr = 
                    std::make_shared<CircularGenome<bool>>(tmp_mutant_genome);
                std::shared_ptr<CircularGenome<bool>::Handler> genome_mutant = 
                    std::dynamic_pointer_cast<CircularGenome<bool>::Handler>(
                        tmp_mutant_genome_ptr->newHandler(tmp_mutant_genome_ptr));
                genome_mutant->resetHandler();
                for(size_t site_idx = 0; site_idx < genome_original->genome->countSites(); ++site_idx){
                    if(site_idx == bit_idx)
                       genome_mutant->writeInt(!genome_original->readInt(0,1), 0, 1);
                    else
                       genome_mutant->writeInt(genome_original->readInt(0,1), 0, 1);
                }

                //// Reevaluate organism after mutation
                org_copy->dataMap.clear("score");
                org_copy->genomes["root::"] = genome_mutant->genome;
                evaluateSolo(org_copy, 0, 0, 0); 
                mutant_data_vec[i].score_mutant = org_copy->dataMap.getAverage("score");
            }
            std::stable_sort(mutant_data_vec.begin(), mutant_data_vec.end(), 
                [](const NKOrgData& a, const NKOrgData& b){
               return a.score_mutant < b.score_mutant; 
            });
            for (int i = 0; i < popSize; i++) {
                rank_vec_mutated[i] = mutant_data_vec[i].rank;
            }
            output_string_stream << Global::update 
                                 << ","
                                 << bit_idx
                                 << ","
                                 << EditDistance(rank_vec_original, rank_vec_mutated, 
                                        (EditDistanceMetric)edit_distance_metric);
            if(bit_idx != N - 1)
                output_string_stream << std::endl;
        }
        FileManager::writeToFile(output_mutant_fitness_filename, output_string_stream.str(), 
            "update,bit_idx,edit_distance");
    } 