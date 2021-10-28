/***********************************************************/
/***      ______  ____  ______                 _         ***/
/***     / ___\ \/ /\ \/ / ___|_ __ __ _ _ __ | |__	     ***/
/***    | |    \  /  \  / |  _| '__/ _` | '_ \| '_ \	 ***/
/***    | |___ /  \  /  \ |_| | | | (_| | |_) | | | |    ***/
/***     \____/_/\_\/_/\_\____|_|  \__,_| .__/|_| |_|    ***/
/***                                    |_|			     ***/
/***********************************************************/
/***     Header-Only C++ Library for Graph			     ***/
/***	 Representation and Algorithms				     ***/
/***********************************************************/
/***     Author: ZigRazor			     			     ***/
/***	 E-Mail: zigrazor@gmail.com 				     ***/
/***********************************************************/
/***	 Collaboration: ----------- 				     ***/
/***********************************************************/
/***	 License: AGPL v3.0							     ***/
/***********************************************************/

#ifndef __CXXGRAPH_PARTITIONING_HDRF_H__
#define __CXXGRAPH_PARTITIONING_HDRF_H__

#pragma once

#include "Partitioning/Utility/Globals.hpp"
#include "Edge/Edge.hpp"
#include "PartitionStrategy.hpp"
#include <chrono>

namespace CXXGRAPH
{
    namespace PARTITIONING
    {
        /**
         * @brief A Vertex Cut Partioning Algorithm ( as described by this paper https://www.fabiopetroni.com/Download/petroni2015HDRF.pdf )
         * @details This algorithm is a greedy algorithm that partitions the graph into n sets of vertices ( as described by this paper https://www.fabiopetroni.com/Download/petroni2015HDRF.pdf ).
         */
        template <typename T>
        class HDRF : public PartitionStrategy<T>
        {
        private:
            Globals GLOBALS;

        public:
            HDRF(Globals &G);
            ~HDRF();

            void performStep(const Edge<T> &e, PartitionState<T> &Sstate);
        };
        template <typename T>
        HDRF<T>::HDRF(Globals &G) : GLOBALS(G)
        {
            //this->GLOBALS = G;
        }
        template <typename T>
        HDRF<T>::~HDRF()
        {
        }
        template <typename T>
        void HDRF<T>::performStep(const Edge<T> &e, PartitionState<T> &state)
        {

            int P = GLOBALS.numberOfPartition;
            int epsilon = 1;
            auto nodePair = e.getNodePair();
            int u = nodePair.first->getId();
            int v = nodePair.second->getId();
            
            Record<T> *u_record = state.getRecord(u);
            Record<T> *v_record = state.getRecord(v);

            //*** ASK FOR LOCK
            bool locks_taken = false;
            while (!locks_taken)
            {

                int usleep_time = 2;
                while (!u_record->getLock())
                {
                    //usleep(usleep_time);
                    std::this_thread::sleep_for(std::chrono::microseconds(usleep_time));
                    usleep_time = (int)pow(usleep_time, 2);
                }
                usleep_time = 2;
                while (!v_record->getLock())
                {
                    usleep(usleep_time);
                    usleep_time = (int)pow(usleep_time, 2);

                    if (usleep_time > GLOBALS.SLEEP_LIMIT)
                    {
                        u_record->releaseLock();
                        //performStep(e, state);
                        //return;
                    } //TO AVOID DEADLOCK
                }
                locks_taken = true;
            }
            //*** LOCK TAKEN
            int machine_id = -1;

            //*** COMPUTE MAX AND MIN LOAD
            int MIN_LOAD = state.getMinLoad();
            int MAX_LOAD = state.getMaxLoad();

            //*** COMPUTE SCORES, FIND MIN SCORE, AND COMPUTE CANDIDATES PARITIONS
            std::vector<int> candidates;
            double MAX_SCORE = 0.0;
            for (int m = 0; m < P; m++)
            {

                int degree_u = u_record->getDegree() + 1;
                int degree_v = v_record->getDegree() + 1;
                int SUM = degree_u + degree_v;
                double fu = 0;
                double fv = 0;
                if (u_record->hasReplicaInPartition(m))
                {
                    fu = degree_u;
                    fu /= SUM;
                    fu = 1 + (1 - fu);
                }
                if (v_record->hasReplicaInPartition(m))
                {
                    fv = degree_v;
                    fv /= SUM;
                    fv = 1 + (1 - fv);
                }
                int load = state.getMachineLoad(m);
                double bal = (MAX_LOAD - load);
                bal /= (epsilon + MAX_LOAD - MIN_LOAD);
                if (bal < 0)
                {
                    bal = 0;
                }
                double SCORE_m = fu + fv + GLOBALS.lambda * bal;
                if (SCORE_m < 0)
                {
                    std::cout << "ERRORE: SCORE_m<0" << std::endl;
                    std::cout << "fu: " << fu << std::endl;
                    std::cout << "fv: " << fv << std::endl;
                    std::cout << "GLOBALS.LAMBDA: " << GLOBALS.lambda << std::endl;
                    std::cout << "bal: " << bal << std::endl;
                    exit(-1);
                }
                if (SCORE_m > MAX_SCORE)
                {
                    MAX_SCORE = SCORE_m;
                    candidates.clear();
                    candidates.push_back(m);
                }
                else if (SCORE_m == MAX_SCORE)
                {
                    candidates.push_back(m);
                }
            }
            //*** CHECK TO AVOID ERRORS
            if (candidates.empty())
            {
                std::cout << "ERROR: GreedyObjectiveFunction.performStep -> candidates.isEmpty()" << std::endl;
                std::cout << "MAX_SCORE: " << MAX_SCORE << std::endl;
                exit(-1);
            }

            //*** PICK A RANDOM ELEMENT FROM CANDIDATES
            /* initialize random seed: */
            unsigned int seed = (unsigned int)time(NULL);
            srand(seed);
            int choice = rand_r(&seed) % candidates.size();
            machine_id = candidates.at(choice);
            try
            {
                CoordinatedPartitionState<T> &cord_state = dynamic_cast<CoordinatedPartitionState<T> &>(state);
                //NEW UPDATE RECORDS RULE TO UPFDATE THE SIZE OF THE PARTITIONS EXPRESSED AS THE NUMBER OF VERTICES THEY CONTAINS
                if (!u_record->hasReplicaInPartition(machine_id))
                {
                    u_record->addPartition(machine_id);
                    cord_state.incrementMachineLoadVertices(machine_id);
                }
                if (!v_record->hasReplicaInPartition(machine_id))
                {
                    v_record->addPartition(machine_id);
                    cord_state.incrementMachineLoadVertices(machine_id);
                }
            }
            catch (std::bad_cast)
            {
                // use employee's member functions
                //1-UPDATE RECORDS
                if (!u_record->hasReplicaInPartition(machine_id))
                {
                    u_record->addPartition(machine_id);
                }
                if (!v_record->hasReplicaInPartition(machine_id))
                {
                    v_record->addPartition(machine_id);
                }
            }

            //2-UPDATE EDGES
            state.incrementMachineLoad(machine_id, &e);

            //3-UPDATE DEGREES
            u_record->incrementDegree();
            v_record->incrementDegree();

            //*** RELEASE LOCK
            u_record->releaseLock();
            v_record->releaseLock();
        }
    }
}

#endif // __CXXGRAPH_PARTITIONING_HDRF_H__