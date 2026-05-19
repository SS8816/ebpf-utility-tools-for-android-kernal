%%%%%%%%% Class file ... do not modify this file ......


\NeedsTeXFormat{LaTeX2e}
\LoadClass[a4paper,12pt]{report}
\RequirePackage{fontenc}
\RequirePackage{setspace}
\RequirePackage{fontenc}
\RequirePackage{graphicx}
\RequirePackage{times}
\usepackage[T1]{fontenc}
\usepackage[left=1in,top=1in,right=1in,bottom=1in]{geometry}
\usepackage{setspace}
\usepackage{graphicx}
\usepackage{ifthen}
\usepackage{xifthen}
\usepackage[shortlabels]{enumitem}
\usepackage[nodayofweek]{datetime}
\usepackage{caption}
\usepackage{array}
\usepackage{hyperref}
\usepackage{times}
\usepackage{float}
\usepackage{amsmath}
\usepackage{blindtext}
\usepackage{sectsty}
\captionsetup[table]{skip=10pt}
\usepackage{array}
\usepackage{tocloft}
\newcolumntype{P}[1]{>{\centering\arraybackslash}p{#1}}
\usepackage{xcolor}


\newtoks\documenttype
\newtoks\department
\newtoks\stream
\newtoks\degree
\newtoks\heading
\newtoks\multiAuthor
\newtoks\nameI
\newtoks\rollI
\newtoks\genderI

\newtoks\nameII
\newtoks\rollII
\newtoks\genderII

\newtoks\nameIII
\newtoks\rollIII
\newtoks\genderIII

\newtoks\nameIV
\newtoks\rollIV
\newtoks\genderIV

\newtoks\batchmonth
\newtoks\batchyear
\newtoks\logo
\newtoks\supervisor
\newtoks\supervisorAffliation
\newtoks\cosupervisor
\newtoks\cosupervisorAffliation
\newtoks\qualification
\newtoks\gateNetPassYear
\newtoks\areaResearch
\newtoks\thisdate
\newtoks\thismonth
\newtoks\thisyear
\newtoks\hod
\newtoks\coordinator
\newtoks\ack
\newtoks\semester
\newtoks\keys
\newtoks\passyear
\newtoks\supervisorpost
\newtoks\supervisordept
\newtoks\supervisorcollege
\newtoks\supervisorcollegeloc
\newtoks\cosupervisorpost
\newtoks\cosupervisordept
\newtoks\cosupervisorcollege
\newtoks\cosupervisorcollegeloc

\newtoks\internalExaminerone
\newtoks\internalExamineronepost
\newtoks\internalExamineronedept
\newtoks\internalExamineronecollege
\newtoks\internalExamineronecollegeloc

\newtoks\internalExaminertwo
\newtoks\internalExaminertwopost
\newtoks\internalExaminertwodept
\newtoks\internalExaminertwocollege
\newtoks\internalExaminertwocollegeloc


\newtoks\external
\newtoks\externalpost
\newtoks\externaldept
\newtoks\externalcollege
\newtoks\externalcollegeloc

\newtoks\sudentGroup
\newtoks\sudentSection


\newtoks\groupcount

\newtoks\roleI
\newtoks\roleII
\newtoks\roleIII
\newtoks\roleIV

\newtoks\signI
\newtoks\signII
\newtoks\signIII
\newtoks\signIV

\newcolumntype{L}[1]{>{\raggedright\arraybackslash}p{#1}}

\providecommand{\keywords}[1]{\noindent\textbf{\textit{\textbf{Keywords: }}} #1}




\newcommand{\details}{

	\vspace{1cm}

	\flushleft
	\bf
	Name: \the\nameI \hfill Supervisor: \the\supervisor \\
	Roll No.: \the\rollI \hfill
	\if\relax\the\cosupervisor\relax
		~
	\else
		Co-supervisor: \the\cosupervisor
	\fi
	\\

}

\newcommand{\groupdetails}{
	\section{NAME, REG. NO AND SIGNATURE OF GROUP MEMBERS:}
	\begin{minipage}{\textwidth}
		\setstretch{2}
		\ifthenelse{\equal{\the\groupcount}{1}}{
			{\bf \centering
					\begin{tabular}{|p{0.06\textwidth}|l|p{0.4\textwidth}|}
						\hline
						1) & \the\nameI~[\the\rollI] & \\\hline
					\end{tabular}
				}
		}{}
		\ifthenelse{\equal{\the\groupcount}{2}}{
			{\bf \centering
					\begin{tabular}{|p{0.06\textwidth}|l|p{0.4\textwidth}|}
						\hline
						1) & \the\nameI~[\the\rollI]   & \\\hline
						2) & \the\nameII~[\the\rollII] & \\\hline
					\end{tabular}
				}
		}{}
		\ifthenelse{\equal{\the\groupcount}{3}}{
			{\bf \centering
					\begin{tabular}{|p{0.06\textwidth}|l|p{0.4\textwidth}|}
						\hline
						1) & \the\nameI~[\the\rollI]     & \\\hline
						2) & \the\nameII~[\the\rollII]   & \\\hline
						3) & \the\nameIII~[\the\rollIII] & \\\hline
					\end{tabular}
				}
		}{}
		\ifthenelse{\equal{\the\groupcount}{4}}{
			{
                    \if\relax\the\signI\relax
            		\bf \centering
    					\begin{tabular}{|p{0.06\textwidth}|l|p{0.4\textwidth}|}
    						\hline
    						1) & \the\nameI~[\the\rollI]     & \\\hline
    						2) & \the\nameII~[\the\rollII]   & \\\hline
    						3) & \the\nameIII~[\the\rollIII] & \\\hline
    						4) & \the\nameIV~[\the\rollIV]   & \\\hline
    					\end{tabular}
            	\else
                            \bf \centering
    					\begin{tabular}{|p{0.06\textwidth}|l|p{0.4\textwidth}|}
    						\hline
    						1) & \the\nameI~[\the\rollI]     & \includegraphics[width=0.4\textwidth, height=1cm]{\the\signI}\\\hline
    						2) & \the\nameII~[\the\rollII]   & \includegraphics[width=0.4\textwidth, height=1cm]{\the\signII}\\\hline
    						3) & \the\nameIII~[\the\rollIII] & \includegraphics[width=0.4\textwidth, height=1cm]{\the\signIII}\\\hline
    						4) & \the\nameIV~[\the\rollIV]   & \includegraphics[width=0.4\textwidth, height=1cm]{\the\signIV}\\\hline
    					\end{tabular}
            	\fi

				
                }
		}{}

	\end{minipage}
}

\newcommand{\approval}{
	\section{\normalsize APPROVAL STATUS (To be filled in by the Section Coordinator of SDP):}

	\vspace{1cm}

	\begin{minipage}{0.3\textwidth}
		\flushleft
		..................................\\
		Project Supervisor
	\end{minipage}\hfill
	\begin{minipage}{0.3\textwidth}
		\centering
		...............................................\\
		Section Coordinator, SDP
	\end{minipage}\hfill
	\begin{minipage}{0.3\textwidth}
		\flushright
		..................................\\
		SDP Coordinator
	\end{minipage}

}

\renewenvironment{abstract}
{

		\cleardoublepage\phantomsection
		\addcontentsline{toc}{section}{Abstract}
		{\centering \bf \Huge Abstract\\~\\}
		
}
{
	\\~\\\keywords{\the\keys}\par\medskip\pagebreak
	\pagebreak

}

\newcommand{\frontpage}{
\ifthenelse{\equal{\the\documenttype}{1}}{
	%				\ifthenelse{\equal{\the\degree}{2}}{
		\renewcommand\thesection{\arabic{section}}
		\renewcommand{\bibname}{References}
		\renewcommand{\cftsecfont}{\bf}
		\patchcmd{\thebibliography}{\chapter*}{\section*}{}{}
		
		\clearpage
		\thispagestyle{empty}
		\begin{minipage}[t][\textheight][t]{\textwidth}
			\setstretch{1.8}
			\centering
			{\Large \bf \expandafter{\the\heading }}\\
			\vfill
			\begin{minipage}{\textwidth}
				\centering
				\setstretch{1.8}
				{\it \large Minor project report submitted in partial fulfilment of the\\
					requirements for the degree of\\}
			\end{minipage}
			\vfill
			\ifthenelse{\equal{\the\degree}{0}}{\large\textbf{{Doctor of Philosophy}}}{}\ifthenelse{\equal{\the\degree}{1}}{\large\textbf{{Master of Technology}}}{}\ifthenelse{\equal{\the\degree}{2}}{\large\textbf{{Bachelor of Technology}}}{}~\\
			\textbf{in}\\\ifthenelse{\equal{\the\stream}{0}}{\large\textbf{{Computer Science \& Engineering}}\\ }{}\ifthenelse{\equal{\the\stream}{1}}{\large\textbf{{Computer Science \& Informatics}}\\}{}\ifthenelse{\equal{\the\stream}{2}}{\large\textbf{{Computer Science \& Data Processing}}\\}{}\ifthenelse{\equal{\the\stream}{4}}{\large\textbf{{Information Technology}}\\}{}
			
			\vfill
			{\large \it by\\}
			\vfill
			\begin{minipage}{\textwidth}
				\centering
				\setstretch{1.2}
				\large \bf
				\ifthenelse{\equal{\the\groupcount}{1}}{
					\the\nameI~(\the\rollI)\\
				}{}
				\ifthenelse{\equal{\the\groupcount}{2}}{
					\the\nameI~(\the\rollI)\\
					\the\nameII~(\the\rollII)\\
				}{}
				\ifthenelse{\equal{\the\groupcount}{3}}{
					\the\nameI~(\the\rollI)\\
					\the\nameII~(\the\rollII)\\
					\the\nameIII~(\the\rollIII)\\
				}{}
				\ifthenelse{\equal{\the\groupcount}{4}}{
					\the\nameI~(\the\rollI)\\
					\the\nameII~(\the\rollII)\\
					\the\nameIII~(\the\rollIII)\\
					\the\nameIV~(\the\rollIV)\\
				}{}
			\end{minipage}
			\vfill
			
			{\large \it under the supervision of\\}
			
			\begin{minipage}{\textwidth}
				\centering
				\setstretch{1.5}
				\if\relax\the\cosupervisor\relax
				\textbf{\the\supervisor}
				\else
				\textbf{\the\supervisor}\\
				\textit{and}\\
				\textbf{\the\cosupervisor}
				\fi\\
			\end{minipage}\\
			\vfill
			\begin{minipage}{\textwidth}
				\centering
				\setstretch{1}
				\includegraphics[width = 40mm, height=40mm]{\the\logo}\\
				%				{ \normalsize \the\thismonth, \the\thisyear}\\
			\end{minipage}\\
			{ \large \the\thismonth, \the\thisyear}
			\vfill
			{\Large {Department of} \expandafter{\the\department}}\\
			%\vfill
			\begin{minipage}{\textwidth}
				\centering
				\setstretch{1.2}
				{\bf \Large {Malaviya National Institute of Technology Jaipur},}\\ {\bf \large Jaipur, Rajasthan, India}\\
			\end{minipage}\\
			%		\vfill
			%			{\footnotesize \copyright~Department of \the\department, MNIT \the\thisyear. All rights reserved}.
		\end{minipage}
		\clearpage
		\pagenumbering{roman}
		\pagestyle{plain}
		
		%					}{}
}{}
		
		\ifthenelse{\equal{\the\documenttype}{2}}{
%				\ifthenelse{\equal{\the\degree}{2}}{
						\renewcommand\thesection{\arabic{section}}
						\renewcommand{\bibname}{References}
						\renewcommand{\cftsecfont}{\bf}
						\patchcmd{\thebibliography}{\chapter*}{\section*}{}{}
			
						\clearpage
						\thispagestyle{empty}
						\begin{minipage}[t][\textheight][t]{\textwidth}
								\setstretch{1.8}
								\centering
								{\Large \bf \expandafter{\the\heading }}\\
								\vfill
								\begin{minipage}{\textwidth}
										\centering
										\setstretch{1.8}
										{\it \large Major Project Report submitted in fulfilment of the\\
										requirements for the award of\\}
									\end{minipage}
								\vfill
								\ifthenelse{\equal{\the\degree}{0}}{\large\textbf{{Doctor of Philosophy}}}{}\ifthenelse{\equal{\the\degree}{1}}{\large\textbf{{Master of Technology}}}{}\ifthenelse{\equal{\the\degree}{2}}{\large\textbf{{Bachelor of Technology}}}{}~\\
								\textbf{in}\\\ifthenelse{\equal{\the\stream}{0}}{\large\textbf{{Computer Science \& Engineering}}\\ }{}\ifthenelse{\equal{\the\stream}{1}}{\large\textbf{{Computer Science \& Informatics}}\\}{}\ifthenelse{\equal{\the\stream}{2}}{\large\textbf{{Computer Science \& Data Processing}}\\}{}\ifthenelse{\equal{\the\stream}{4}}{\large\textbf{{Information Technology}}\\}{}
								
								\vfill
								{\large \it by\\}
								\vfill
								\begin{minipage}{\textwidth}
										\centering
										\setstretch{1.2}
										\large \bf
										\ifthenelse{\equal{\the\groupcount}{1}}{
												\the\nameI~(\the\rollI)\\
											}{}
										\ifthenelse{\equal{\the\groupcount}{2}}{
												\the\nameI~(\the\rollI)\\
												\the\nameII~(\the\rollII)\\
											}{}
										\ifthenelse{\equal{\the\groupcount}{3}}{
												\the\nameI~(\the\rollI)\\
												\the\nameII~(\the\rollII)\\
												\the\nameIII~(\the\rollIII)\\
											}{}
										\ifthenelse{\equal{\the\groupcount}{4}}{
												\the\nameI~(\the\rollI)\\
												\the\nameII~(\the\rollII)\\
												\the\nameIII~(\the\rollIII)\\
												\the\nameIV~(\the\rollIV)\\
											}{}
									\end{minipage}
								\vfill
								
								{\large \it under the supervision of\\}
								
								\begin{minipage}{\textwidth}
									\centering
									\setstretch{1.5}
									\if\relax\the\cosupervisor\relax
									\textbf{\the\supervisor}
									\else
									\textbf{\the\supervisor}\\
									\textit{and}\\
									\textbf{\the\cosupervisor}
									\fi\\
								\end{minipage}\\
								\vfill
								\begin{minipage}{\textwidth}
										\centering
										\setstretch{1}
										\includegraphics[width = 40mm, height=40mm]{\the\logo}\\
										%				{ \normalsize \the\thismonth, \the\thisyear}\\
									\end{minipage}\\
								{ \bf \large \the\thismonth, \the\thisyear}
								\vfill
								{\large \bf {Department of} \expandafter{\the\department}}\\
								\vfill
								\begin{minipage}{\textwidth}
										\centering
										\setstretch{1.2}
										{\bf \Large {Malaviya National Institute of Technology Jaipur},}\\ {\bf \large Jaipur, Rajasthan, India}\\
									\end{minipage}\\
								%		\vfill
								%			{\footnotesize \copyright~Department of \the\department, MNIT \the\thisyear. All rights reserved}.
							\end{minipage}
						\clearpage
						\pagenumbering{roman}
						\pagestyle{plain}
			
%					}{}
			}{}
	
}





\newcommand{\roleauthor}{
	\ifthenelse{\equal{\the\documenttype}{3}}{
		\ifthenelse{\equal{\the\degree}{2}}
		{
			\cleardoublepage\phantomsection
			\addcontentsline{toc}{section}{INDIVIDUAL CONTRIBUTIONS}
			\noindent\begin{minipage}{\textwidth}
				\begin{center}
					{\centering\bf\Large INDIVIDUAL CONTRIBUTIONS}
				\end{center}

				\vspace{1cm}
			\end{minipage}~\\

			\noindent\begin{minipage}{\textwidth}
				\setstretch{2}
				\ifthenelse{\equal{\the\groupcount}{1}}{
					\begin{tabular}{|p{0.35\textwidth}|L{0.65\textwidth}|}
						\hline
						\the\nameI & \the\roleI \\
						\hline
					\end{tabular}
				}{}
				\ifthenelse{\equal{\the\groupcount}{2}}{
					\begin{tabular}{|p{0.35\textwidth}|L{0.65\textwidth}|}
						\hline
						\the\nameI  & \the\roleI  \\\hline
						\the\nameII & \the\roleII \\
						\hline
					\end{tabular}
				}{}
				\ifthenelse{\equal{\the\groupcount}{3}}{
					\begin{tabular}{|p{0.35\textwidth}|L{0.65\textwidth}|}
						\hline
						\the\nameI   & \the\roleI   \\\hline
						\the\nameII  & \the\roleII  \\\hline
						\the\nameIII & \the\roleIII \\
						\hline
					\end{tabular}
				}{}
				\ifthenelse{\equal{\the\groupcount}{4}}{
					\begin{tabular}{|p{0.35\textwidth}|L{0.65\textwidth}|}
						\hline
						\the\nameI   & \the\roleI   \\\hline
						\the\nameII  & \the\roleII  \\\hline
						\the\nameIII & \the\roleIII \\\hline
						\the\nameIV  & \the\roleIV  \\
						\hline
					\end{tabular}

				}{}

			\end{minipage}\\\pagebreak
		}
		{}
	}{}

}


\newcommand{\reportapproval}{
	\ifthenelse{\equal{\the\documenttype}{3}}{
		\ifthenelse{\equal{\the\degree}{2}}
		{
			\cleardoublepage\phantomsection
			\addcontentsline{toc}{section}{REPORT APPROVAL}
			\noindent\begin{minipage}{\textwidth}
				\begin{center}
					{\centering\bf\Large REPORT APPROVAL}
				\end{center}

				\vspace{1cm}
			\end{minipage}~\\

			\noindent\begin{minipage}{\textwidth}
				\setstretch{1.5}
				This project report entitled \textbf{\uppercase\expandafter{\the\heading}} by \ifthenelse{\equal{\the\groupcount}{1}}{{\bf \the\nameI}}{}\ifthenelse{\equal{\the\groupcount}{2}}{{\bf \the\nameI, and \the\nameII}}{}
				\ifthenelse{\equal{\the\groupcount}{3}}{{\bf \the\nameI, \the\nameII, and \the\nameIII}}{}\ifthenelse{\equal{\the\groupcount}{4}}{{\bf \the\nameI, \the\nameII, \the\nameIII, and \the\nameIV}}{} is
				approved for the degree of \ifthenelse{\equal{\the\degree}{0}}{\textbf{Ph. D.}}{}\ifthenelse{\equal{\the\degree}{1}}{\textbf{M. Tech.}}{}\ifthenelse{\equal{\the\degree}{2}}{\textit{Bachelor of Technology}}{}~in~\ifthenelse{\equal{\the\stream}{0}}{\textit{Computer Science \& Engineering}}{}\ifthenelse{\equal{\the\stream}{1}}{\textit{Computer Science \& Informatics}}{}\ifthenelse{\equal{\the\stream}{2}}{\textit{Computer Science \& Data Processing}}{}\ifthenelse{\equal{\the\stream}{4}}{\textit{Information Technology}}{}.
			\end{minipage}\\

			\vspace{1cm}

			\noindent\begin{minipage}{\textwidth}
				\setstretch{2.5}
				\large
				Examiner(s)\\
				\underline{~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~}\\
				\underline{~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~}\\
				\underline{~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~}\\~\\
				Supervisor\\
				\underline{~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~}\\~\\
				Project Coordinator\\
				\underline{~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~}\\
			\end{minipage}\\
		}
		{}
	}{}

}

\newcommand{\declaration}{

		\cleardoublepage\phantomsection
		\addcontentsline{toc}{section}{Declaration}
		\begin{minipage}{\textwidth}
			{{\centering\bf\Huge Declaration of Originality\\~\\}}
			\setstretch{1.5}
			\ifthenelse{\equal{\the\groupcount}{1}}{I}{We} hereby declare that this work entitled \textbf{\expandafter{\the\heading}} presents \ifthenelse{\equal{\the\groupcount}{1}}{my}{our} original work carried out
			as a \ifthenelse{\equal{\the\degree}{0}}{{{doctroal}}}{}\ifthenelse{\equal{\the\degree}{1}}{{{postgraduate}}}{}\ifthenelse{\equal{\the\degree}{2}}{{{under graduate}}}{} student of MNIT Jaipur and, to the best of \ifthenelse{\equal{\the\groupcount}{1}}{my}{our} knowledge, contains no
			material previously published or written by another person, nor any material presented by
			\ifthenelse{\equal{\the\groupcount}{1}}{m}{us} for the award of any degree or diploma of MNIT Jaipur or any other institution. Any relevant material taken from the the work of others (whether published or unpublished) has been properly acknowledged and referenced in accordance with the institute's guidelines.

			\textbf{AI Disclosure:} This text was written by us and edited for clarity and grammar using AI assistance (Google Gemini). The core content, ideas, arguments, structural decisions, and intellectual property remain our own.
		\end{minipage}

		\vspace{2cm}

\noindent\begin{minipage}[t]{0.45\textwidth}
	Place: MNIT, Jaipur\\~\\
	Date : \the\thismonth \ \the\thisdate, \the\thisyear\\
\end{minipage}\hfill\begin{minipage}[t]{0.45\textwidth}
	\flushright
	Signature of students\\
	\setstretch{3}
	\ifthenelse{\equal{\the\groupcount}{1}}{
		\begin{tabular}{p{0.4\textwidth}l}
			& (\the\rollI) \\\cline{1-1}
		\end{tabular}
	}{}
	\ifthenelse{\equal{\the\groupcount}{2}}{
		\begin{tabular}{p{0.4\textwidth}l}
			& (\the\rollI)  \\\cline{1-1}
			& (\the\rollII) \\\cline{1-1}
		\end{tabular}
		
	}{}
	\ifthenelse{\equal{\the\groupcount}{3}}{
		\begin{tabular}{p{0.4\textwidth}l}
			& (\the\rollI)   \\\cline{1-1}
			& (\the\rollII)  \\\cline{1-1}
			& (\the\rollIII) \\\cline{1-1}
		\end{tabular}
	}{}
	\ifthenelse{\equal{\the\groupcount}{4}}{
		\begin{tabular}{p{0.4\textwidth}l}
			& (\the\rollI)   \\\cline{1-1}
			& (\the\rollII)  \\\cline{1-1}
			& (\the\rollIII) \\\cline{1-1}
			& (\the\rollIV)  \\\cline{1-1}
		\end{tabular}
		
	}{}
\end{minipage}
	\pagebreak
}



\newcommand{\certificate}{
			\cleardoublepage\phantomsection
			\addcontentsline{toc}{section}{Certificate}
			\noindent\begin{minipage}{\textwidth}
				\begin{minipage}{0.2\textwidth}
					{\includegraphics[width = 25mm, height=25mm]{\the\logo}}
				\end{minipage}\begin{minipage}[t][][b]{0.8\textwidth}
				\setstretch{1.5}
				{\large Department of \ifthenelse{\equal{\the\stream}{0}}{{Computer Science \& Engineering}\\ }{}\ifthenelse{\equal{\the\stream}{1}}{{Computer Science \& Informatics}\\}{}\ifthenelse{\equal{\the\stream}{2}}{{Computer Science \& Data Processing}\\}{}\ifthenelse{\equal{\the\stream}{4}}{{Information Technology}\\}{}}
				{\Large \bf Malaviya National Institute of Technology Jaipur}
			\end{minipage}
		\par\noindent\rule{\textwidth}{2pt}~\\
				\begin{center}
					
					\vfill
					{\centering \bf\Large Supervisor's Certificate}
				\end{center}
				\vspace{0.5cm}
			\end{minipage}~\\
			\begin{minipage}{\textwidth}
				\hyphenpenalty=10000
				\setstretch{1.5}
				\noindent This is to certify that the project report titled \textbf{\expandafter{\the\heading}}
				is being submitted by \ifthenelse{\equal{\the\groupcount}{1}}{{\bf \the\nameI ~(\the\rollI)}}{}\ifthenelse{\equal{\the\groupcount}{2}}{{\bf \the\nameI ~(\the\rollI), and \the\nameII ~(\the\rollII)}}{}
				\ifthenelse{\equal{\the\groupcount}{3}}{{\bf \the\nameI ~(\the\rollI), \the\nameII ~(\the\rollII), and \the\nameIII ~(\the\rollIII)}}{}\ifthenelse{\equal{\the\groupcount}{4}}{{\bf \the\nameI ~(\the\rollI), \the\nameII ~(\the\rollII), \the\nameIII ~(\the\rollIII), and \the\nameIV ~(\the\rollIV)}}{},
				is a record of original research carried out by \ifthenelse{\equal{\the\groupcount}{1}}{\ifthenelse{\equal{\the\genderI}{male}}{him}{her}}{them} under my supervision and guidance in partial fulfillment of the requirements of the degree of \ifthenelse{\equal{\the\degree}{0}}{{{Doctor of Philosophy}}}{}\ifthenelse{\equal{\the\degree}{1}}{{{Master of Technology}}}{}\ifthenelse{\equal{\the\degree}{2}}{{{Bachelor of Technology}}}{} in \ifthenelse{\equal{\the\stream}{0}}{{{Computer Science \& Engineering}}}{}\ifthenelse{\equal{\the\stream}{1}}{{{Computer Science \& Informatics}}}{}\ifthenelse{\equal{\the\stream}{2}}{{{Computer Science \& Data Processing}}}{}\ifthenelse{\equal{\the\stream}{4}}{{{Information Technology}}}{}. Neither this report nor any part of it has been submitted earlier for
				any degree or diploma to any institute or university in India or abroad.\\
			\end{minipage}\\

			\vspace{2cm}

			\noindent\begin{minipage}{\textwidth}
				\setstretch{1}
				\textbf{\the\supervisor}\\~\\
				Department of \the\supervisordept,\\\the\supervisorcollege,\\\the\supervisorcollegeloc
			\end{minipage}\hfill\begin{minipage}{0.5\textwidth}
				\setstretch{1}
				\flushright
				\if\relax\the\cosupervisor\relax
					~\\
				\else
					\the\cosupervisor\\
					(Co-supervisor)\\
					Department of \the\cosupervisordept,\\\the\cosupervisorcollege,\\\the\cosupervisorcollegeloc
				\fi
			\end{minipage}

	\pagebreak

}

\newcommand{\acknowledgment}{
			\cleardoublepage\phantomsection
			\addcontentsline{toc}{section}{Acknowledgement}
			\noindent\begin{minipage}{\textwidth}
				\begin{center}
					{\centering \bf\Huge Acknowledgement}
				\end{center}
				\vspace{1cm}
			\end{minipage}~\\
			\begin{minipage}{\textwidth}
				\setstretch{1.5}
				\hyphenpenalty=10000
				\the\ack
			\end{minipage}\\

			\vspace{2cm}

			\noindent\begin{minipage}[t]{0.45\textwidth}
				Place: MNIT, Jaipur\\~\\
				Date :  \the\thismonth \ \the\thisdate,  \the\thisyear\\
			\end{minipage}\hfill\begin{minipage}[t]{0.45\textwidth}
				\flushright
				Signature of students\\
				\setstretch{3}
				\ifthenelse{\equal{\the\groupcount}{1}}{
					\begin{tabular}{p{0.4\textwidth}l}
						& (\the\rollI) \\\cline{1-1}
					\end{tabular}
				}{}
				\ifthenelse{\equal{\the\groupcount}{2}}{
					\begin{tabular}{p{0.4\textwidth}l}
						& (\the\rollI)  \\\cline{1-1}
						& (\the\rollII) \\\cline{1-1}
					\end{tabular}
					
				}{}
				\ifthenelse{\equal{\the\groupcount}{3}}{
					\begin{tabular}{p{0.4\textwidth}l}
						& (\the\rollI)   \\\cline{1-1}
						& (\the\rollII)  \\\cline{1-1}
						& (\the\rollIII) \\\cline{1-1}
					\end{tabular}
				}{}
				\ifthenelse{\equal{\the\groupcount}{4}}{
					\begin{tabular}{p{0.4\textwidth}l}
						& (\the\rollI)   \\\cline{1-1}
						& (\the\rollII)  \\\cline{1-1}
						& (\the\rollIII) \\\cline{1-1}
						& (\the\rollIV)  \\\cline{1-1}
					\end{tabular}
					
				}{}
			\end{minipage}

}

\newcommand{\thesisapproval}{
	\ifthenelse{\equal{\the\documenttype}{3}}{

		\cleardoublepage\phantomsection
		\addcontentsline{toc}{section}{THESIS APPROVAL}
		\noindent\begin{minipage}{\textwidth}
			\begin{center}
				\chapter*{\centering \bf\Large THESIS APPROVAL}
			\end{center}
			\begin{center}
				\setstretch{0.2}
				\rule{\textwidth}{2.5pt}\\
				\rule{\textwidth}{1pt}\\
			\end{center}
			\vspace{1cm}
		\end{minipage}~\\
		\begin{minipage}{\textwidth}
			\setstretch{1.5}
			\hyphenpenalty=10000

			This \ifthenelse{\equal{\the\degree}{0}}{dissertation}{thesis} entitled \textbf{\the\heading} by \textbf{\the\nameI~(\the\rollI)} is approved for the degree of \ifthenelse{\equal{\the\degree}{0}}{\textbf{Ph. D.}}{}\ifthenelse{\equal{\the\degree}{1}}{\textbf{M. Tech.}}{}\ifthenelse{\equal{\the\degree}{2}}{\textbf{B. Tech.}}{}~in~\ifthenelse{\equal{\the\stream}{0}}{\textbf{Computer Science \& Engineering} }{}\ifthenelse{\equal{\the\stream}{1}}{\textbf{Computer Science \& Informatics} }{}\ifthenelse{\equal{\the\stream}{2}}{\textbf{Computer Science \& Data Processing} }{}\ifthenelse{\equal{\the\stream}{4}}{\textbf{Information Technology}}{}.
		\end{minipage}

		\vfill

		\noindent\begin{minipage}{\textwidth}
			\flushleft
			\rule{7cm}{0.4pt}\\
			{\bf Supervisor:} \the\supervisor , \the\supervisorpost, \the\supervisorcollege.
		\end{minipage}
		\vfill
		\noindent\begin{minipage}{\textwidth}
			\flushleft
			\if\relax\the\cosupervisor\relax
				~\\
			\else
				\rule{7cm}{0.4pt}\\
				{\bf Co-Supervisor:} \the\cosupervisor , \the\cosupervisorpost, \the\cosupervisorcollege.
			\fi
		\end{minipage}
		\vfill
		\noindent\begin{minipage}{\textwidth}
			\flushleft
			\if\relax\the\internalExaminerone\relax
				~\\
			\else
				\rule{7cm}{0.4pt}\\
				{\bf Internal Examiner:} \the\internalExaminerone , \the\internalExamineronepost, \the\internalExamineronecollege.
			\fi
		\end{minipage}
		\vfill
		\noindent\begin{minipage}{\textwidth}
			\flushleft
			\if\relax\the\internalExaminertwo\relax
				~\\
			\else
				\rule{7cm}{0.4pt}\\
				{\bf Internal Examiner:} \the\internalExaminertwo , \the\internalExaminertwopost, \the\internalExaminertwocollege.
			\fi
		\end{minipage}
		\vfill
		\noindent\begin{minipage}{\textwidth}
			\flushleft
			\if\relax\the\external\relax
				~\\
			\else
				\rule{7cm}{0.4pt}\\
				{\bf External Examiner:} \the\external , \the\externalpost, \the\externalcollege.
			\fi
		\end{minipage}


		\pagebreak

	}{}

}


\newcommand{\addcontentpages}{
			\addcontentsline{toc}{section}{Contents}
			\renewcommand*\contentsname{\bf \noindent\begin{minipage}{\textwidth}
					\begin{center}
						\vspace{-4.5cm}~\\
						{\bf\Huge Contents}\\
					\end{center}
				\end{minipage}\vspace{-3.5cm}~\\}
			\tableofcontents\pagebreak
			\cleardoublepage\phantomsection
			\addcontentsline{toc}{section}{List of Figures}
			\renewcommand*\listfigurename{
				\noindent\begin{minipage}{\textwidth}
					\begin{center}
						\vspace{-3.5cm}~\\
						{\bf\Huge List of Figures}\\~\\\vspace{-2cm}~\\
						\hrule
						\vspace{0.2cm}
						{\bf\normalsize ~~No\hfill Figure Name\hfill Page No }\\\vspace{0.3cm}\hrule~\\
					\end{center}
				\end{minipage}\vspace{-2.5cm}~\\}
			\listoffigures\pagebreak
			\cleardoublepage\phantomsection
			\addcontentsline{toc}{section}{List of Tables}
			\renewcommand*\listtablename{
			\noindent\begin{minipage}{\textwidth}
				\begin{center}
					\vspace{-3.5cm}~\\
					{\bf\Huge List of Tables}\\~\\\vspace{-2cm}~\\
					\hrule
					\vspace{0.2cm}
					{\bf\normalsize ~~No\hfill Table Name\hfill Page No }\\\vspace{0.3cm}\hrule~\\
				\end{center}
			\end{minipage}\vspace{-2.5cm}~\\}
			\listoftables\pagebreak
}

%%%%%%%%% Written by: Deepak Ranjan Nayak
%%%%%%%%% email: drnayak.cse@mnit.ac.in
%%%%%%%%% last modified : 01-Dec-2025
