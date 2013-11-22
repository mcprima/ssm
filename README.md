S|S|M
=====

Inference for time series analysis with *S*tate *S*pace *M*odels like
playing with duplo blocks.

    cat guess.json | ./simplex -M 10000 | ./ksimplex -M 10000 > best_fit.json
    cat best_fit.json | ./kmcmc -M 100000 | ./pmcmc -J 1000 -M 500000 --trace > yeah_i_am_done.json

[![NPM](https://nodei.co/npm/ssm.png)](https://nodei.co/npm/ssm/)

Installation
============

All the methods are implemented in C. The C code contain generic part
(working with any models) and model specific part.  The specific parts
are templated using Python and [SymPy](http://sympy.org/) for symbolic
mathematics. [JavaScript](https://brendaneich.com/brendaneich_content/uploads/CapitolJS.021.png)
is used to glue things together and add features on top of the C core.

## Installing the required dependencies

C:
- [gsl](http://www.gnu.org/software/gsl/) (>= 1.15)
- [zmq](http://www.zeromq.org/) (3.2 release)
- [jansson](http://www.digip.org/jansson/) (>= 2.4)

Python:
- [Python 2.7.x](www.python.org/)
- [Jinja2](http://jinja.pocoo.org/docs/)
- [SymPy](http://sympy.org/)
- [dateutil](http://labix.org/python-dateutil)

On OSX with [homebrew](http://mxcl.github.io/homebrew/) and [pip](https://pypi.python.org/pypi/pip):

    brew install jansson zmq gsl
    sudo pip install jinja2 sympy python-dateutil

On Ubuntu:

    apt-get update
    apt-get install -y python-software-properties python g++ make build-essential
    add-apt-repository -y ppa:chris-lea/zeromq
    apt-get update
    apt-get install -y libzmq-dev libjansson-dev python-sympy python-jinja2 python-dateutil libgsl0-dev
 

## Installing S|S|M itself

with [npm](http://nodejs.org/)

    npm install -g ssm

Note: requires that all the C and python dependencies have been
installed _before_ as this will also build the standalone C libraries.

Pull requests are welcome for a .gyp file and windows support!

We also recomend that you install [jsontool](http://trentm.com/json/)

    npm install -g jsontool


Tests
=====

    npm test

Notes:
The C code is tested with [clar](https://github.com/vmg/clar)

Usage
=====

## Data and parameters

Data have to be in [SDF](http://dataprotocols.org/simple-data-format/)
and wrapped in a
[datapackage](http://dataprotocols.org/data-packages/).

For instance a [CSV](http://tools.ietf.org/html/rfc4180) file

    $ head data/data.csv
    
    "date","cases"
    "2012-08-02",5
    "2012-08-09",5
    "2012-08-16",6
    "2012-08-23",12
    "2012-08-30",null

will be wrapped as follows:

    $ cat package.json | json resources
    
    "resources": [
      {
        "name": "data",
        "path": "data/data.csv",
        "format": "csv",
        "schema": {
          "fields": [
            {"name": "date", "type": "date"},
            {"name": "cases", "type": "number"}
          ]
        }
      },
      ...
    ]

Parameters also have to be specified as resources of a datapackage
(the same or another one).
For instance the following resource defines a prior.

    $ cat package.json | json resources

    "resources": [
      {
        "name": "pr_v",
        "description": "duration of infection",
        "format": "json",
        "data": { 
          "distribution": "normal", 
          "mean": 12.5,
          "sd": 3.8265, 
          "lower": 0.0, 
          "unit": "days"
        }
      },
      ...
    ]

The initial values of the parameters and the covariance matrix between
them need also to be defined as resources of a datapackage.

    $ cat package.json | json resources

    "resources": [
      {
        "name": "values",
        "description": "initial values for the parameters",
        "format": "json",
        "data": {
          "r0": 25.0,
          "pr_v": 11.0
        }
      },
      {
        "name": "covariance",
        "description": "covariance matrix (only the diagonal terms are mandatory)",
        "format": "json",
        "data": {
          "r0": {"r0": 0.04, "pr_v": 0.01},
          "pr_v": {"pr_v": 0.02, "r0": 0.01}
        }
      },
      ...
    ]

## Model

A model is described in [JSON](http://www.json.org/) and typicaly
lives as a metadata of a datapackage. S|S|M support any State Space
Model.  A model is defined in a model object (```"model": {}```).

Let's take the example of a compartmental model for population
dynamics. the ```model``` object contains the following properties:

the populations (required only for population dynamics)

    $ cat package.json | json model.populations

    "populations": [
      {"name": "NYC", "composition": ["S", "I", "R"]}
    ]

the reactions, defining the process model

    $ cat package.json | json model.reactions

    "reactions": [
      {"from": "S", "to": "I", "rate": "r0/(S+I+R)*v*I", "description": "infection", "tracked": ["Inc"]},
      {"from": "I", "to": "R", "rate": "v", "description":"recovery"}
    ]

you can also defined SDE and ODE in addition / in place of reactions.

the observation model

    $ cat package.json | json model.observations

    "observations": [
      {
        "name": "cases",
        "start": "2012-07-26",
        "distribution": "discretized_normal",
        "mean": "rep * Inc",
        "sd": "sqrt(rep * ( 1.0 - rep ) * Inc )"
      }
    ]

some link to the data (that can live in another datapackage)

    $ cat package.json | json model.data
    
    "data": [
      { 
        "name": "cases", 
        "data": [ {"resource": "data", "field": "date"}, {"resource": "data", "field": "cases"} ] 
      }
    ]

some link to the parameters (that can alse live in another datapackage).

    $ cat package.json | json model.inputs
    
    "inputs": [
      { "name": "r0", "description": "basic reproduction number", "data": {"resource": "r0"} },
      { "name": "v",  "description": "recovery rate", "data": {"resource":  "pr_v"}, "transformation": "1/pr_v", "to_resource": "1/v" },
      { "name": "S", "description": "Number of susceptible", "data": {"resource": "S"} },
      { "name": "I", "description": "Number of infectious", "data": {"resource": "I"} },
      { "name": "R", "description": "Number of recovered", "data": {"resource": "R"} },
      { "name": "rep", "description": "reporting rate", "data": {"resource": "rep"} }
    ]

Note that this linking stage also allow to include some _transformations_.

Full examples are available in the examples directory (```examples/sir/package.json``` for this one).


## Installing a model from a data package

    $ ssm install package.json [options]

This will build several inference and simulation methods
([MIF](http://www.pnas.org/content/103/49/18438),
[pMCMC](http://onlinelibrary.wiley.com/doi/10.1111/j.1467-9868.2009.00736.x/abstract),
[simplex](http://en.wikipedia.org/wiki/Nelder%E2%80%93Mead_method),
[SMC](http://en.wikipedia.org/wiki/Particle_filter),
[Kalman filters](http://en.wikipedia.org/wiki/Kalman_filter), ...)
customized to different implementation of you model
([ode](http://en.wikipedia.org/wiki/Ordinary_differential_equation),
[sde](http://en.wikipedia.org/wiki/Stochastic_differential_equation),
[poisson process with stochastic rates](http://arxiv.org/pdf/0802.0021.pdf),
...).

All the methods are ready as is for *parallel computing* (using
multiple core of a machine _and_ leveraging a cluster of machines).

Run ```method --help``` to get help and see the different
implementations and options supported by the method.


## Inference like playing with duplo blocks

Let's plot the data

with [R](http://www.r-project.org/):

     data <- read.csv('../data/data.csv', na.strings='null')
     plot(as.Date(data$date), data$cases, type='s')

Let's run a first simulation:

     $ cat ../package.json | ./simul --traj

And add the simulated trajectory to our first plot

     traj <- read.csv('X_0.csv')
     lines(as.Date(traj$date), traj$cases, type='s', col='red')

Let's infer the parameters to get a better fit

     $ cat ../package.json | ./simplex -M 10000 --trace > mle.json

let's read the values found:

     $ cat mle.json | json resources | json -c "this.name=='values'"
     [
       {
         "format": "json",
         "name": "values",
         "data": {
           "pr_v": 19.379285906561037,
           "r0": 29.528755614881494
         }
       }
     ]

Let's plot the evolution of the parameters:

     trace <- read.csv('trace_0.csv')
     layout(matrix(1:3,1,3))
     plot(trace$index, trace$r0, type='l')
     plot(trace$index, trace$pr_v, type='l')
     plot(trace$index, trace$fitness, type='l')


Now let's redo a simulation with these values (```mle.json```):

     $ cat mle.json | ./simul --traj -v

and replot the results:

     plot(as.Date(data$date), data$cases, type='s')
     traj <- read.csv('X_0.csv')
     lines(as.Date(traj$date), traj$cases, type='s', col='red')

much better.

And now in one line:

    $ cat ../package.json | ./simplex -M 10000 --trace | ./simul --traj | json resources | json -c "this.name=='values'"
    [
      {
        "name": "values",
        "format": "json",
        "data": {
          "r0": 29.528755614881494,
          "pr_v": 19.379285906561037
        }
      }
    ]
    

Let's get some posteriors and sample some trajectories by adding a
pmcmc at the end of our pipeline (we actualy add 2 of them to skip
some transiant).

     $ cat ../package.json | ./simplex -M 10000 | ./pmcmc -M 10000 | ./pmcmc -M 100000 --trace --traj  | json resources | json -c 'this.name=="summary"'
     
     [
       {
         "format": "json",
         "name": "summary",
         "data": {
           "log_ltp": -186.70579009197556,
           "AICc": 363.94320971360844,
           "n_parameters": 2,
           "AIC": 363.6765430469418,
           "DIC": 363.6802334782078,
           "log_likelihood": -179.8382715234709,
           "sum_squares": null,
           "n_data": 48
         }
       }
     ]

Some posteriors plots (still with R)

     trace <- read.csv('trace_0.csv')
     layout(matrix(1:2,1,2))
     hist(trace$r0)
     hist(trace$pr_v)

The sampled trajectories

     traj <- read.csv('X_0.csv')
     plot(as.Date(data$date), data$cases, type='s')
     samples <- unique(traj$index)
     for(i in samples){
       lines(as.Date(traj$date[traj$index == i]), traj$cases[traj$index == i], type='s', col='red')
     }

## Be cautious

Always validate your results blah blah blah... Package like
[CODA](http://cran.r-project.org/web/packages/coda/index.html) are
here to help.

## Inference pipelines

For more advanced cases (like running in parallel a lot of runs, each
starting from different initial conditions, selecting the best of this
runs and restarting from that with another algorithm...) inference
pipelines are here to help:

    $ ssm bootstrap package.json [options]

This will produce a data package (```ssm_model/package.json```). Open it and customize it for your
analysis. When ready just fire:

    $ ssm run ssm_model/package.json [options]

## Parallel computing

Let's say that you want to run a particle filter of a stochastic
version of our previous model with 1000 particles in you 4 cores
machines (```--n_thread```). Also instead of plotting 1000
trajectories you just want a summary of the empirical confindence
envelop (```--hat```).

    $ cat ../package.json | ./smc psr -J 1000 --n_thread 4 --hat

Let's plot the trajectories

    hat <- read.csv('hat_0.csv')
    plot(as.Date(hat$date), hat$mean_cases, type='s')
    lines(as.Date(hat$date), hat$lower_cases, type='s', lty=2)
    lines(as.Date(hat$date), hat$upper_cases, type='s', lty=2)

Your machine is not enough ? Let's use several.  First let's transform
our ```smc``` into a server that will dispatch some work to several
workers (living in different machines).

    $ cat ../package.json | ./smc psr -J 1000 --tcp

All the algorithm shipped with S|S|M can be transformed into servers
with the ```--tcp``` option.

Now let's start some workers giving them the adress of the server.

    $ cat ../package.json | ./worker psr smc --server 127.0.0.1 &
    $ cat ../package.json | ./worker psr smc --server 127.0.0.1 &

Note that you can add workers at any time during a run.

License
=======

GPL version 3 or any later version.
