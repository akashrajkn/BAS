% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/plot.coef.bas.R
\name{plot.coef.bas}
\alias{plot.coef.bas}
\title{Plots the posterior distributions of coefficients derived from Bayesian
model averaging}
\usage{
\method{plot}{coef.bas}(x, e = 1e-04, subset = 1:x$n.vars, ask = TRUE,
  ...)
}
\arguments{
\item{x}{object of class coef.bas}

\item{e}{optional numeric value specifying the range over which the
distributions are to be graphed.}

\item{subset}{optional numerical vector specifying which variables to graph
(including the intercept)}

\item{ask}{Prompt for next plot}

\item{...}{other parameters to be passed to \code{plot} and \code{lines}}
}
\description{
Displays plots of the posterior distributions of the coefficients generated
by Bayesian model averaging over linear regression.
}
\details{
Produces plots of the posterior distributions of the coefficients under
model averaging.  The posterior probability that the coefficient is zero is
represented by a solid line at zero, with height equal to the probability.
The nonzero part of the distribution is scaled so that the maximum height is
equal to the probability that the coefficient is nonzero.

The parameter \code{e} specifies the range over which the distributions are
to be graphed by specifying the tail probabilities that dictate the range to
plot over.
}
\note{
For mixtures of g-priors, uncertainty in g is not incorporated at this
time, thus results are approximate
}
\examples{

\dontrun{library(MASS)
data(UScrime)
UScrime[,-2] = log(UScrime[,-2])
crime.bic = bas.lm(y ~ ., data=UScrime, n.models=2^15, prior="BIC")
plot(coefficients(crime.bic), ask=TRUE)
}

}
\references{
Hoeting, J.A., Raftery, A.E. and Madigan, D. (1996). A method
for simultaneous variable selection and outlier identification in linear
regression. Computational Statistics and Data Analysis, 22, 251-270.
}
\seealso{
\code{ \link{coef.bas}}

Other bas plots: \code{\link{image.bas}},
  \code{\link{plot.bas}}
}
\author{
based on function \code{plot.bic} by Ian Painter in package BMA;
adapted for 'bas' class by Merlise Clyde \email{clyde@stat.duke.edu}
}
\keyword{regression}
