
#include "common.h"
// THIS FUNCTION PRINTS THE CURRENT HAPLOTYPE ASSEMBLY in a new file block by block
extern int DISCRETE_PRUNING;
extern int ERROR_ANALYSIS_MODE;
extern int SKIP_PRUNE;
extern float THRESHOLD;

void print_hapcut_options() {
    fprintf(stdout, "\nHapCUT2: robust and accurate haplotype assembly for diverse sequencing technologies\n\n");
    fprintf(stdout, "USAGE : ./HAPCUT2 --fragments fragment_file --vcf variantcalls.vcf --output haplotype_output_file\n\n");
    fprintf(stderr, "Basic Options:\n");
    fprintf(stderr, "--fragments, --f <FILENAME>:        file with haplotype-informative reads generated using the extracthairs program\n");
    fprintf(stderr, "--vcf <FILENAME>:                   variant file in VCF format (use EXACT SAME file that was used for the extracthairs program)\n");
    fprintf(stderr, "--output, --o <FILENAME> :          file to which phased haplotype segments/blocks will be output\n");
    fprintf(stderr, "--converge, --c <int>:              cut off iterations (global or maxcut) after this many iterations with no improvement. default: 5\n");
    fprintf(stderr, "--verbose, --v <0/1>:               verbose mode: print extra information to stdout and stderr. default: 0\n");

    fprintf(stderr, "\nRead Technology Options:\n");
    fprintf(stderr, "--hic <0/1> :                       increases accuracy on HiC data; models h-trans errors directly from the data. default: 0\n");
    fprintf(stderr, "--hic_htrans_file, --hf <FILENAME>  optional tab-delimited input file where second column specifies h-trans error probabilities for insert size bins 0-50Kb, 50Kb-100Kb, etc.\n");
    fprintf(stderr, "--qv_offset, --qo <33/48/64> :      quality value offset for base quality scores, default: 33 (use same value as for extracthairs)\n");
    fprintf(stderr, "--long_reads, --lr <0/1> :          reduces memory when phasing long read data with many SNPs per read. default: automatic.\n");

    fprintf(stderr, "\nHaplotype Post-Processing Options:\n");
    fprintf(stderr, "--threshold, --t <float>:           threshold for pruning low-confidence SNPs (closer to 1 prunes more, closer to 0.5 prunes less). default: 0.8\n");
    fprintf(stderr, "--split_blocks, --sb <0/1>:         split blocks using simple likelihood score to reduce switch errors. default: 0\n");
    fprintf(stderr, "--split_threshold, --st <float>:    threshold for splitting blocks (closer to 1 splits more, closer to 0.5 splits less). default: 0.8\n");
    fprintf(stderr, "--call_homozygous, --ch <0/1>:      call positions as homozygous if they appear to be false heterozygotes. default: 0\n");
    fprintf(stderr, "--discrete_pruning, --dp <0/1>:     use discrete heuristic to prune SNPs. default: 0\n");
    fprintf(stderr, "--error_analysis_mode, --ea <0/1>:  print confidence scores to haplotype file but don't split blocks or prune. default: 0\n");

    fprintf(stderr, "\nAdvanced Options:\n");
    fprintf(stderr, "--new_format, --nf <0/1>:           use new HiC fragment matrix file format (but don't do h-trans error modeling). default: 0\n");
    fprintf(stderr, "--max_iter, --mi <int> :            maximum number of global iterations. Preferable to tweak --converge option instead. default: 10000\n");
    fprintf(stderr, "--maxcut_iter, --mc <int> :         maximum number of max-likelihood-cut iterations. Preferable to tweak --converge option instead. default: 10000\n");
    fprintf(stderr, "--htrans_read_lowbound, --hrl <int> with --HiC on, h-trans probability estimation will require this many matepairs per window. default: 500\n");
    fprintf(stderr, "--htrans_max_window, --hmw <int>    with --HiC on, the insert-size window for h-trans probability estimation will not expand larger than this many basepairs. default: 4000000\n");

    fprintf(stderr, "\n\nHiC-specific Notes:\n");
    fprintf(stderr, "  (1) When running extractHAIRS, must use --HiC 1 option to create a fragment matrix in the new HiC format.\n");
    fprintf(stderr, "  (2) When running HapCUT2, use --HiC 1 if h-trans probabilities are unknown. Use --HiC_htrans_file if they are known\n");
    fprintf(stderr, "  (3) Using --HiC_htrans_file is faster than --HiC and may yield better results at low read coverage (>30x).\n");
    fprintf(stderr, "  (4) Set --converge to a larger value if possible/reasonable.\n ");
    fprintf(stderr, "\n");

}

int print_hapfile(struct BLOCK* clist, int blocks, char* h1, struct fragment* Flist, int fragments, struct SNPfrags* snpfrag, char* fname, int score, char* outfile) {
    // print a new file containing one block phasing and the corresponding fragments
    int i = 0, t = 0, k = 0, span = 0;
    char c=0, c1=0, c2=0;
    //char fn[200]; sprintf(fn,"%s-%d.phase",fname,score);
    FILE* fp;
    fp = fopen(outfile, "w");

    for (i = 0; i < blocks; i++) {
        span = snpfrag[clist[i].lastvar].position - snpfrag[clist[i].offset].position;
        fprintf(fp, "BLOCK: offset: %d len: %d phased: %d ", clist[i].offset + 1, clist[i].length, clist[i].phased);
        fprintf(fp, "SPAN: %d fragments %d\n", span, clist[i].frags);
        for (k = 0; k < clist[i].phased; k++) {

            t = clist[i].slist[k];
            if (h1[t] =='0')
                c= '1';
            else if (h1[t] =='1')
                c = '0';
            else c = h1[t];
            // print this line to keep consistency with old format
            // if SNP was pruned then print '-'s
            if ((!ERROR_ANALYSIS_MODE)&&(!SKIP_PRUNE)
                &&((snpfrag[t].post_hap < log10(THRESHOLD) && !DISCRETE_PRUNING)
                ||(snpfrag[t].pruned_discrete_heuristic && DISCRETE_PRUNING))){
                fprintf(fp, "%d\t-\t-\t", t + 1);
            }else if (snpfrag[t].genotypes[0] == '0' && snpfrag[t].genotypes[2] == '0'){
                fprintf(fp, "%d\t0\t0\t", t + 1);   // homozygous 00
            }else if (snpfrag[t].genotypes[0] == '1' && snpfrag[t].genotypes[2] == '1'){
                fprintf(fp, "%d\t1\t1\t", t + 1);   // homozygous 11
            }else if (snpfrag[t].genotypes[0] == '2' || snpfrag[t].genotypes[2] == '2') {

                if (h1[t] == '0') {
                    c1 = snpfrag[t].genotypes[0];
                    c2 = snpfrag[t].genotypes[2];
                } else if (h1[t] == '1') {
                    c2 = snpfrag[t].genotypes[0];
                    c1 = snpfrag[t].genotypes[2];
                }
                fprintf(fp, "%d\t%c\t%c\t", t + 1, c1, c2); // two alleles that are phased in VCF like format
            } else {
                fprintf(fp, "%d\t%c\t%c\t", t + 1, h1[t], c);
            }


            fprintf(fp, "%s\t%d\t%s\t%s\t%s\t%d\t%0.6f\t%0.6f\n", snpfrag[t].chromosome, snpfrag[t].position, snpfrag[t].allele0, snpfrag[t].allele1, snpfrag[t].genotypes, snpfrag[t].pruned_discrete_heuristic, snpfrag[t].post_notsw, snpfrag[t].post_hap);

        }
        if (i < blocks - 1) fprintf(fp, "******** \n");
    }
    fclose(fp);
    return 1;
}

// important NOTE: to get from SNP i to its component in 'clist', we have variable snpfrag[i].bcomp, feb 1 2012

void print_haplotypes_vcf(struct BLOCK* clist, int blocks, char* h1, struct fragment* Flist, int fragments, struct SNPfrags* snpfrag, int snps, char* outfile) {
    // print the haplotypes in VCF like format
    FILE* fp;
    fp = fopen(outfile, "w");
    int i = 0, k = 0, span = 0, component;
    char c1, c2;
    for (i = 0; i < snps; i++) {
        fprintf(fp, "%s\t%d\t%s\t%s\t%s\t", snpfrag[i].chromosome, snpfrag[i].position, snpfrag[i].id, snpfrag[i].allele0, snpfrag[i].allele1);
        fprintf(fp, "%s\t", snpfrag[i].genotypes);
        component = snpfrag[i].component;
        if (component < 0) fprintf(fp, "./.\n");
        else if (snpfrag[component].csize < 2) fprintf(fp, "./.\n");
        else {
            k = snpfrag[i].bcomp;
            if (clist[k].offset == i) // main node of component
                //if (component ==i) // main node of component changed feb 14 2013
            {
                if (h1[i] == '0') {
                    c1 = '0';
                    c2 = '1';
                } else {
                    c1 = '1';
                    c2 = '0';
                }
                span = snpfrag[clist[k].lastvar].position - snpfrag[clist[k].offset].position;
                fprintf(fp, "SP:%c|%c:%d ", c1, c2, clist[k].offset + 1);
                fprintf(fp, "BLOCKlength: %d phased %d SPAN: %d MECscore %2.2f fragments %d\n", clist[k].length, clist[k].phased, span, clist[k].bestSCORE, clist[k].frags);
            } else {
                if (h1[i] == '0') {
                    c1 = '0';
                    c2 = '1';
                } else {
                    c1 = '1';
                    c2 = '0';
                }
                fprintf(fp, "SP:%c|%c:%d\n", c1, c2, clist[k].offset + 1);
            }
        }
    }
}
