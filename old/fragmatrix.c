#include "fragmatrix.h"
#include "printhaplotypes.c"
#include "find_starting_haplotypes.c"
#include "MECscore.c"
#define MAXBUF 10000

// new functions for connected components using disjoint union data structure feb 14 2013 

int find_parent(struct SNPfrags* snpfrag, int x) {
    // all nodes on path to root will be labeled with the same id
    if (snpfrag[x].component != x) snpfrag[x].component = find_parent(snpfrag, snpfrag[x].component);
    return snpfrag[x].component;
}

// disjoint union data structure where for each node we maintain information about its parent
// following parent to parent pointers -> information about connected component (i)

int determine_connected_components(struct fragment* Flist, int fragments, struct SNPfrags* snpfrag, int snps) {
    int i = 0, j = 0, k = 0, x = 0, y = 0, parent_x = 0, parent_y = 0;
    for (i = 0; i < snps; i++) {
        snpfrag[i].component = i; // initialize every node to a singleton set
        snpfrag[i].rank = 0;
        snpfrag[i].csize = 1;
    }

    // each fragment is a set of nodes that should end up in the same connected component 
    for (i = 0; i < fragments; i++) {
        for (j = 0; j < Flist[i].blocks; j++) {
            for (k = 0; k < Flist[i].list[j].len; k++) {
                if (j == 0 && k == 0) continue;
                x = Flist[i].list[0].offset;
                y = Flist[i].list[j].offset + k; // the pair of SNPs
                parent_x = find_parent(snpfrag, x);
                parent_y = find_parent(snpfrag, y);
                if (parent_x == parent_y) continue;
                else if (snpfrag[parent_x].rank < snpfrag[parent_y].rank) {
                    snpfrag[parent_x].component = parent_y;
                    snpfrag[parent_y].csize += snpfrag[parent_x].csize;
                    snpfrag[parent_x].csize = 0;
                } else if (snpfrag[parent_x].rank > snpfrag[parent_y].rank) {
                    snpfrag[parent_y].component = parent_x;
                    snpfrag[parent_x].csize += snpfrag[parent_y].csize;
                    snpfrag[parent_y].csize = 0;
                } else {
                    snpfrag[parent_y].component = parent_x;
                    snpfrag[parent_x].rank++;
                    snpfrag[parent_x].csize += snpfrag[parent_y].csize;
                    snpfrag[parent_y].csize = 0;
                }
            }
        }
    }

    int components = 0, singletons = 0;
    for (i = 0; i < snps; i++) {
        if (snpfrag[i].component == i && snpfrag[i].csize > 1) (components)++;
        else if (snpfrag[i].component == i) (singletons)++;
    }
    fprintf(stdout, " no of non-trivial connected components in graph is %d singletons %d mean size %0.1f\n", components, singletons, (float) (snps - singletons) / components);
    fprintf(stderr, " no of non-trivial connected components in graph is %d singletons %d mean size %0.1f\n", components, singletons, (float) (snps - singletons) / components);
    return components;
}

// populate the connected component data structure only for non-trivial connected components, at least 2 variants

void generate_clist_structure(struct fragment* Flist, int fragments, struct SNPfrags* snpfrag, int snps, int components, struct BLOCK* clist) {
    // bcomp maps the SNP to the component number in clist since components << snps  
    // note that the previous invariant about the first SNP (ordered by position) being the root of the component is no longer true !! 
    // should we still require it ?? 
    int i = 0, component = 0;
    for (i = 0; i < snps; i++) snpfrag[i].bcomp = -1;
    for (i = 0; i < snps; i++) {
        if (snpfrag[i].component == i && snpfrag[i].csize > 1) // root node of component
        {
            snpfrag[i].bcomp = component;
            clist[component].slist = calloc(sizeof (int), snpfrag[i].csize);
            clist[component].phased = 0;
            component++;
        }
    }
    //fprintf(stderr,"non-trivial components in graph %d \n",components);
    for (i = 0; i < snps; i++) {
        if (snpfrag[i].component < 0) continue; // to allow for initialization to -1 in other code feb 15 2013
        if (snpfrag[i].csize <= 1 && snpfrag[i].component == i) continue; // ignore singletons that are not connected to other variants

        if (snpfrag[i].component != i) snpfrag[i].bcomp = snpfrag[snpfrag[i].component].bcomp;
        if (snpfrag[i].bcomp < 0) continue;
        component = snpfrag[i].bcomp;
        if (clist[component].phased == 0) clist[component].offset = i;
        clist[component].slist[clist[component].phased] = i;
        clist[component].phased++;
        clist[component].lastvar = i;
    }
    for (i = 0; i < components; i++) clist[i].length = clist[i].lastvar - clist[i].offset + 1;
    for (i = 0; i < components; i++) clist[i].frags = 0;
    for (i = 0; i < fragments; i++) {
        if (snpfrag[Flist[i].list[0].offset].bcomp < 0)continue; // ignore fragments that cover singleton vertices
        clist[snpfrag[Flist[i].list[0].offset].bcomp].frags++;
    }
    for (i = 0; i < components; i++) clist[i].flist = calloc(sizeof (int), clist[i].frags);
    for (i = 0; i < components; i++) clist[i].frags = 0;
    for (i = 0; i < fragments; i++) {
        if (snpfrag[Flist[i].list[0].offset].bcomp < 0)continue;
        clist[snpfrag[Flist[i].list[0].offset].bcomp].flist[clist[snpfrag[Flist[i].list[0].offset].bcomp].frags] = i;
        clist[snpfrag[Flist[i].list[0].offset].bcomp].frags++;
    }
    for (i = 0; i < components; i++) {
        fprintf(stdout, "comp %d first %d last %d phased %d fragments %d \n", i, clist[i].offset, clist[i].lastvar, clist[i].phased, clist[i].frags);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief this function updates the data structure snpfrag which links the heterozyous SNPs and the haplotype fragments
 * it generates a list of fragments (flist) that affect each SNP
 * 
 * @param Flist the fragment array, read from the fragment matrix inputfile
 * @param fragments the size of Flist
 * @param snpfrag a pointer to  the SNPfrag data structure to be populated by this function
 * @param snps the total number of SNPs in the dataset
 */
void update_snpfrags(struct fragment* Flist, int fragments, struct SNPfrags* snpfrag, int snps) {
    int i = 0, j = 0, k = 0, calls = 0; //maxdeg=0,avgdeg=0;

    // find the first fragment whose endpoint lies at snp 'i' or beyond
    
    // iterate over all SNPs and set a default snpfrag value
    for (i = 0; i < snps; i++) {
        snpfrag[i].frags = 0;
        snpfrag[i].ff = -1;
    }
    
    // iterate over all fragments
    for (i = 0; i < fragments; i++) {
        //FIXME: Why is this being done for all fragments?
        // j and k are just being reassigned, so shouldn't it just be
        // done a single time for i=fragments-1?
        j = Flist[i].list[0].offset;
        k = Flist[i].list[Flist[i].blocks - 1].len + Flist[i].list[Flist[i].blocks - 1].offset;
        // commented the line below since it slows program for long mate-pairs june 7 2012
        //for (t=j;t<k;t++) { if (snpfrag[t].ff == -1) snpfrag[t].ff = i;  } 
    } //for (i=0;i<snps;i++) { fprintf(stdout,"SNP %d firstfrag %d start snp %d \n",i,snpfrag[i].ff,i); } 

    // for each fragment, for each block, for each base, increment the fragment count at SNP position
    for (i = 0; i < fragments; i++) {
        for (j = 0; j < Flist[i].blocks; j++) {
            //if (Flist[i].list[j].offset+k < 0 || Flist[i].list[j].offset+k >= snps) fprintf(stdout,"%d %d %s\n",Flist[i].list[j].offset,snps,Flist[i].id);
            for (k = 0; k < Flist[i].list[j].len; k++){
                snpfrag[Flist[i].list[j].offset + k].frags++;
            }
        }
    }
    
    // for each SNPfrag (one for each SNP), allocate array sizes big enough
    // to hold indices for fragments and allele identities
    for (i = 0; i < snps; i++) {
        snpfrag[i].flist = (int*) malloc(sizeof (int)*snpfrag[i].frags);
        snpfrag[i].alist = (char*) malloc(snpfrag[i].frags);
    }

    // initialize SNPfrags with default values
    for (i = 0; i < snps; i++) {
        snpfrag[i].component = -1;
        snpfrag[i].csize = 1;
        snpfrag[i].frags = 0;
        snpfrag[i].edges = 0;
        snpfrag[i].best_mec = 10000;
    }

    // for each fragment, for each block, for each base, set appropriate data in SNPfrag:
    // flist (array of indices of fragments overlapping a column)
    // alist (array of allele identities at each of these overlapping fragments)
    // frags (number of frags overlapping)
    // 
    for (i = 0; i < fragments; i++) {
        calls = 0;
        for (j = 0; j < Flist[i].blocks; j++) {
            for (k = 0; k < Flist[i].list[j].len; k++) {
                snpfrag[Flist[i].list[j].offset + k].flist[snpfrag[Flist[i].list[j].offset + k].frags] = i;
                snpfrag[Flist[i].list[j].offset + k].alist[snpfrag[Flist[i].list[j].offset + k].frags] = Flist[i].list[j].hap[k];
                snpfrag[Flist[i].list[j].offset + k].frags++;
                calls += Flist[i].list[j].len;
            }
        }
        // for each SNP in the fragment -> number of edges can be incremented by size of fragment -1, feb 14 2013
        // non-fosmid data
        if (FOSMIDS == 0) {
            for (j = 0; j < Flist[i].blocks; j++) {
                for (k = 0; k < Flist[i].list[j].len; k++){
                    snpfrag[Flist[i].list[j].offset + k].edges += calls - 1;
                }
            }
        // fosmid data
        } else if (FOSMIDS >= 1) {
            for (j = 0; j < Flist[i].blocks; j++) {
                for (k = 0; k < Flist[i].list[j].len; k++){
                    // ??? Why 4 here
                    snpfrag[Flist[i].list[j].offset + k].edges += 4;
                }
            }
            // only one edge for first and last nodes
            j = 0;
            snpfrag[Flist[i].list[j].offset].edges -= 1;
            j = Flist[i].blocks - 1;
            snpfrag[Flist[i].list[j].offset + Flist[i].list[j].len - 1].edges -= 1;
        }
    }
}