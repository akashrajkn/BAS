
#include "sampling.h"
void insert_model_tree(struct Node *tree, struct Var *vars,  int n, int *model, int num_models);
void update_tree(SEXP modelspace, struct Node *tree, SEXP modeldim, struct Var *vars, int k, int p, int n, int kt, int *model);
int *GetModel_m(SEXP Rmodel_m, int *model, int p);
double FitModel(SEXP Rcoef_m, SEXP Rse_m, double *XtY, double *XtX, int *model_m,
				double *XtYwork, double *XtXwork, double yty, double SSY, int pmodel, int p,
				int nobs, int m, double *pmse_m);
SEXP gglm_lpy(SEXP RX, SEXP RY,SEXP Ra, SEXP Rb, SEXP Rs, SEXP Rcoef, SEXP Rmu);

void SetModel2(double logmargy, double shrinkage_m, double prior_m,
			   SEXP sampleprobs, SEXP logmarg, SEXP shrinkage, SEXP priorprobs, int m);
void SetModel1(SEXP Rfit, SEXP Rmodel_m, 
			   SEXP beta, SEXP se, SEXP modelspace, SEXP deviance, SEXP R2, SEXP Q, int m);
double GetNextModelCandidate(int pmodel_old, int n, int n_sure, int *model, struct Var *vars, double problocal,
							 int *varin, int *varout);
void CreateTree(NODEPTR branch, struct Var *vars, int *bestmodel, int *model, int n, int m, SEXP modeldim);
void CreateTree_with_pigamma(NODEPTR branch, struct Var *vars, int *bestmodel, int *model, int n, int m, 
							 SEXP modeldim, double *pigamma);
void Substract_visited_probability_mass(NODEPTR branch, struct Var *vars, int *model, int n, int m,  
										double *pigamma, double eps);
void GetNextModel_swop(NODEPTR branch, struct Var *vars, int *model, int n, int m,  double *pigamma, 
					   double problocal, SEXP modeldim,int *bestmodel);

SEXP glm_FitModel(SEXP RX, SEXP RY, SEXP Rmodel_m,  //input data
				  SEXP Roffset, SEXP Rweights, SEXP family, SEXP Rcontrol,
				  SEXP Ra, SEXP Rb, SEXP Rs);
SEXP glm_mcmcbas(SEXP Y, SEXP X, SEXP Roffset, SEXP Rweights, 
				 SEXP Rprobinit, SEXP Rmodeldim, 
				 SEXP modelprior, SEXP Rbestmodel,  SEXP Rbestmarg,SEXP plocal, 
				 SEXP BURNIN_Iterations,
				 SEXP Ra, SEXP Rb, SEXP Rs,
				 SEXP family, SEXP Rcontrol,
				 SEXP Rupdate
				 ) 
{

	int nProtected = 0;
	int nModels=LENGTH(Rmodeldim);
	SEXP ANS = PROTECT(allocVector(VECSXP, 16)); ++nProtected;
	SEXP ANS_names = PROTECT(allocVector(STRSXP, 16)); ++nProtected;
	SEXP Rprobs = PROTECT(duplicate(Rprobinit)); ++nProtected;
	SEXP MCMCprobs= PROTECT(duplicate(Rprobinit)); ++nProtected;
	SEXP R2 = PROTECT(allocVector(REALSXP, nModels)); ++nProtected;
	SEXP shrinkage = PROTECT(allocVector(REALSXP, nModels)); ++nProtected;
	SEXP modelspace = PROTECT(allocVector(VECSXP, nModels)); ++nProtected;
	SEXP modeldim =  PROTECT(duplicate(Rmodeldim)); ++nProtected;
	SEXP counts =  PROTECT(duplicate(Rmodeldim)); ++nProtected;
	SEXP beta = PROTECT(allocVector(VECSXP, nModels)); ++nProtected;
	SEXP se = PROTECT(allocVector(VECSXP, nModels)); ++nProtected;
	SEXP deviance = PROTECT(allocVector(REALSXP, nModels)); ++nProtected;
	SEXP modelprobs = PROTECT(allocVector(REALSXP, nModels)); ++nProtected;
	SEXP priorprobs = PROTECT(allocVector(REALSXP, nModels)); ++nProtected;
	SEXP logmarg = PROTECT(allocVector(REALSXP, nModels)); ++nProtected;
	SEXP sampleprobs = PROTECT(allocVector(REALSXP, nModels)); ++nProtected;
	SEXP Q = PROTECT(allocVector(REALSXP, nModels)); ++nProtected;

	SEXP NumUnique = PROTECT(allocVector(INTSXP, 1)); ++nProtected;

	double *probs, MH=0.0, prior_m=1.0, logmargy, postold, postnew;
	int i, m, n, pmodel_old, *bestmodel;
	int mcurrent, n_sure;

	//get dimsensions of all variables 
	int p = INTEGER(getAttrib(X,R_DimSymbol))[1];
	int k = LENGTH(modelprobs);
	int update = INTEGER(Rupdate)[0];
	double eps = DBL_EPSILON;

	struct Var *vars = (struct Var *) R_alloc(p, sizeof(struct Var)); // Info about the model variables. 
	probs =  REAL(Rprobs);
	n = sortvars(vars, probs, p); 
	for (i =n; i <p; i++) REAL(MCMCprobs)[vars[i].index] = probs[vars[i].index];
	for (i =0; i <n; i++) REAL(MCMCprobs)[vars[i].index] = 0.0;

	// fill in the sure things 
	int *model = ivecalloc(p);
	for (i = n, n_sure = 0; i < p; i++)  {
		model[vars[i].index] = (int) vars[i].prob;
		if (model[vars[i].index] == 1) ++n_sure;
	}

	GetRNGstate();

	NODEPTR tree, branch;
	tree = make_node(-1.0);
	//  Rprintf("For m=0, Initialize Tree with initial Model\n");  

	m = 0;
	bestmodel = INTEGER(Rbestmodel);
	INTEGER(modeldim)[m] = n_sure;

	// Rprintf("Create Tree\n"); 
	branch = tree;
	CreateTree(branch, vars, bestmodel, model, n, m, modeldim);
	int pmodel = INTEGER(modeldim)[m];
	SEXP Rmodel_m =	PROTECT(allocVector(INTSXP,pmodel));
	GetModel_m(Rmodel_m, model, p);
	//evaluate logmargy and shrinkage
	SEXP glm_fit = PROTECT(glm_FitModel(X, Y, Rmodel_m, Roffset, Rweights, family, Rcontrol, Ra, Rb, Rs));	
	prior_m  = compute_prior_probs(model,pmodel,p, modelprior);

	logmargy = REAL(getListElement(getListElement(glm_fit, "lpy"),"lpY"))[0];
	SetModel2(logmargy, NA_REAL, prior_m, sampleprobs, logmarg, shrinkage, priorprobs, m);
	SetModel1(glm_fit, Rmodel_m, beta, se, modelspace, deviance, R2, Q, m);
	UNPROTECT(2);

	int nUnique=0, newmodel=0;
	double *real_model = vecalloc(n);
	int *modelold = ivecalloc(p);
	int old_loc = 0;
	int new_loc;
	pmodel_old = pmodel;
	nUnique=1;
	INTEGER(counts)[0] = 0;
	postold =  REAL(logmarg)[m] + log(REAL(priorprobs)[m]);
	memcpy(modelold, model, sizeof(int)*p);
	m = 0;
	int *varin= ivecalloc(p);
	int *varout= ivecalloc(p);
	double problocal = REAL(plocal)[0];
	while (nUnique < k && m < INTEGER(BURNIN_Iterations)[0]) {
		memcpy(model, modelold, sizeof(int)*p);
		pmodel =  n_sure;

		MH = GetNextModelCandidate(pmodel_old, n, n_sure, model, vars, problocal, varin, varout);

		branch = tree;
		newmodel= 0;
		for (i = 0; i< n; i++) {
			int bit =  model[vars[i].index];
			if (bit == 1) {
				if (branch->one != NULL) branch = branch->one;
				else newmodel = 1;
			} else {
				if (branch->zero != NULL)  branch = branch->zero;
				else newmodel = 1;
			} 
			pmodel  += bit;
		}

		if (pmodel  == n_sure || pmodel == n + n_sure) {
			MH = 1.0/(1.0 - problocal);
		}
		if (newmodel == 1) {
			new_loc = nUnique;
			PROTECT(Rmodel_m = allocVector(INTSXP,pmodel));
			GetModel_m(Rmodel_m, model, p);

			glm_fit = PROTECT(glm_FitModel(X, Y, Rmodel_m, Roffset, Rweights, family, Rcontrol, Ra, Rb, Rs));	
			prior_m = compute_prior_probs(model,pmodel,p, modelprior);

			logmargy = REAL(getListElement(getListElement(glm_fit, "lpy"),"lpY"))[0];
			postnew = logmargy + log(prior_m);
		} else {
			new_loc = branch->where;
			postnew =  REAL(logmarg)[new_loc] + log(REAL(priorprobs)[new_loc]);      
		} 

		MH *= exp(postnew - postold);
		//    Rprintf("MH new %lf old %lf\n", postnew, postold);
		if (unif_rand() < MH) {
			if (newmodel == 1)  {
				new_loc = nUnique;
				insert_model_tree(tree, vars, n, model, nUnique);
				INTEGER(modeldim)[nUnique] = pmodel;
				//Rprintf("model %d: %d variables\n", m, pmodel);
				SetModel2(logmargy, NA_REAL, prior_m, sampleprobs, logmarg, shrinkage, priorprobs, nUnique);
				SetModel1(glm_fit, Rmodel_m, beta, se, modelspace, deviance, R2, Q, nUnique);
				UNPROTECT(2);	
				++nUnique; 
			}
			old_loc = new_loc;
			postold = postnew;
			pmodel_old = pmodel;
			memcpy(modelold, model, sizeof(int)*p);
		} else  {
			if (newmodel == 1) UNPROTECT(2);
		}
		INTEGER(counts)[old_loc] += 1;
		for (i = 0; i < n; i++) {
			// store in opposite order so nth variable is first 
			real_model[n-1-i] = (double) modelold[vars[i].index];
			REAL(MCMCprobs)[vars[i].index] += (double) modelold[vars[i].index];
		}
		m++;
	}

	for (i = 0; i < n; i++) {
		REAL(MCMCprobs)[vars[i].index] /= (double) m;
	}

	// Compute marginal probabilities  
	mcurrent = nUnique;
	Rprintf("NumUnique Models Accepted %d \n", nUnique);
	compute_modelprobs(modelprobs, logmarg, priorprobs,mcurrent);
	compute_margprobs(modelspace, modeldim, modelprobs, probs, mcurrent, p);        

	//  Now sample W/O Replacement  
	INTEGER(NumUnique)[0] = nUnique;
	
	if (nUnique < k) {
		int *modelwork= ivecalloc(p);
		double *pigamma = vecalloc(p);
		update_probs(probs, vars, mcurrent, k, p);
		update_tree(modelspace, tree, modeldim, vars, k,p,n,mcurrent, modelwork);     
		for (m = nUnique;  m < k; m++) {
			for (i = n; i < p; i++)  {
				INTEGER(modeldim)[m]  +=  model[vars[i].index];
			}

			branch = tree;
			GetNextModel_swop(branch, vars, model, n, m, pigamma, problocal, modeldim, bestmodel);

			/* Now subtract off the visited probability mass. */
			branch=tree;
			Substract_visited_probability_mass(branch, vars, model, n, m, pigamma,eps);

			/* Now get model specific calculations */
			pmodel = INTEGER(modeldim)[m];
			PROTECT(Rmodel_m = allocVector(INTSXP,pmodel));
			GetModel_m(Rmodel_m, model, p);

			glm_fit = PROTECT(glm_FitModel(X, Y, Rmodel_m, Roffset, Rweights, family, Rcontrol, Ra, Rb, Rs));	
			prior_m = compute_prior_probs(model,pmodel,p, modelprior);
			logmargy = REAL(getListElement(getListElement(glm_fit, "lpy"),"lpY"))[0];
			SetModel2(logmargy, NA_REAL, prior_m, sampleprobs, logmarg, shrinkage, priorprobs, m);
			SetModel1(glm_fit, Rmodel_m, beta, se, modelspace, deviance, R2, Q, m);
			UNPROTECT(2);

			REAL(sampleprobs)[m] = pigamma[0];  

			//update best model
			if (REAL(logmarg)[m] > REAL(Rbestmarg)[0]) {
				for (i=0; i < p; i++) {
					bestmodel[i] = model[i];
				}
				REAL(Rbestmarg)[0] = REAL(logmarg)[m];
			}

			//update marginal inclusion probs
			if (m > 1) {
				double mod; 
				double rem = modf((double) m/(double) update, &mod);
				if (rem  == 0.0) {
					int mcurrent = m;
					compute_modelprobs(modelprobs, logmarg, priorprobs,mcurrent);
					compute_margprobs(modelspace, modeldim, modelprobs, probs, mcurrent, p);        
					if (update_probs(probs, vars, mcurrent, k, p) == 1) {
						Rprintf("Updating Model Tree %d \n", m);
						update_tree(modelspace, tree, modeldim, vars, k,p,n,mcurrent, modelwork);     
					}
				}
			}  
		}
	}

	compute_modelprobs(modelprobs, logmarg, priorprobs,k);
	compute_margprobs(modelspace, modeldim, modelprobs, probs, k, p);  

	INTEGER(NumUnique)[0] = nUnique;
	SET_VECTOR_ELT(ANS, 0, Rprobs);
	SET_STRING_ELT(ANS_names, 0, mkChar("probne0"));

	SET_VECTOR_ELT(ANS, 1, modelspace);
	SET_STRING_ELT(ANS_names, 1, mkChar("which"));

	SET_VECTOR_ELT(ANS, 2, logmarg);
	SET_STRING_ELT(ANS_names, 2, mkChar("logmarg"));

	SET_VECTOR_ELT(ANS, 3, modelprobs);
	SET_STRING_ELT(ANS_names, 3, mkChar("postprobs"));

	SET_VECTOR_ELT(ANS, 4, priorprobs);
	SET_STRING_ELT(ANS_names, 4, mkChar("priorprobs"));

	SET_VECTOR_ELT(ANS, 5, sampleprobs);
	SET_STRING_ELT(ANS_names, 5, mkChar("sampleprobs"));

	SET_VECTOR_ELT(ANS, 6, deviance);
	SET_STRING_ELT(ANS_names, 6, mkChar("deviance"));

	SET_VECTOR_ELT(ANS, 7, beta);
	SET_STRING_ELT(ANS_names, 7, mkChar("coefficients"));

	SET_VECTOR_ELT(ANS, 8, se);
	SET_STRING_ELT(ANS_names, 8, mkChar("se"));

	SET_VECTOR_ELT(ANS, 9, shrinkage);
	SET_STRING_ELT(ANS_names, 9, mkChar("shrinkage"));

	SET_VECTOR_ELT(ANS, 10, modeldim);
	SET_STRING_ELT(ANS_names, 10, mkChar("size"));

	SET_VECTOR_ELT(ANS, 11, R2);
	SET_STRING_ELT(ANS_names, 11, mkChar("R2"));

	SET_VECTOR_ELT(ANS, 12, counts);
	SET_STRING_ELT(ANS_names, 12, mkChar("freq"));

	SET_VECTOR_ELT(ANS, 13, MCMCprobs);
	SET_STRING_ELT(ANS_names, 13, mkChar("probs.MCMC"));

	SET_VECTOR_ELT(ANS, 14, NumUnique);
	SET_STRING_ELT(ANS_names, 14, mkChar("n.Unique"));

	SET_VECTOR_ELT(ANS, 15, Q);
	SET_STRING_ELT(ANS_names, 15, mkChar("Q"));

	setAttrib(ANS, R_NamesSymbol, ANS_names);
	
	PutRNGstate();
    UNPROTECT(nProtected);
	return(ANS);  
}

